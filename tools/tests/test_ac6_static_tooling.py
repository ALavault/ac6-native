from __future__ import annotations

import hashlib
import copy
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from ac6_mode1_codec import descramble
from ac6_fhm import parse_fhm
from audit_ac6_mission01_native_gate import GateError, audit_contract, template
from build_ac6_asset_closure import build_closure
from compare_ac6_asset_closures import compare as compare_closures
from compare_ac6_function_snapshots import compare_three
from extract_ac6_pac import extract_selected


def legacy_template() -> dict:
    document = template()
    old = copy.deepcopy(document)
    old["schema"] = "ac6.mission01-native-gate.v1"
    old["requirements"] = {
        **old["requirements"]["J0"],
        "units_and_waves": old["requirements"]["domains"]["native_units_and_waves"],
        "targeting": {"status": "open", "statement": "targeting", "evidence": []},
        "weapons": {"status": "open", "statement": "weapons", "evidence": []},
        "damage_and_destruction": {"status": "open", "statement": "damage and destruction", "evidence": []},
        "retail_objectives": old["requirements"]["domains"]["retail_objectives"],
        "essential_hud": old["requirements"]["domains"]["essential_hud"],
        "scenario_radio_or_subtitles": old["requirements"]["domains"]["scenario_radio_or_subtitles"],
        "success_failure_debrief": old["requirements"]["domains"]["success_failure_debrief"],
        "pause_save_restart": old["requirements"]["runtime"]["pause_save_restart"],
    }
    return old


def make_fhm(children: list[bytes]) -> bytes:
    count = len(children)
    table_end = 0x14 + 8 * count
    header_size = max(0x18, table_end)
    offsets: list[int] = []
    cursor = header_size
    for child in children:
        offsets.append(cursor)
        cursor += len(child)
    blob = bytearray(cursor)
    blob[:4] = b"FHM "
    struct.pack_into(">I", blob, 0x10, count)
    for index, offset in enumerate(offsets):
        struct.pack_into(">I", blob, 0x14 + index * 4, offset)
    sizes_begin = 0x14 + count * 4
    for index, child in enumerate(children):
        struct.pack_into(">I", blob, sizes_begin + index * 4, len(child))
        blob[offsets[index] : offsets[index] + len(child)] = child
    return bytes(blob)


def write_extract_manifest(root: Path, entry_index: int, payload: bytes) -> Path:
    payload_dir = root / "payloads"
    payload_dir.mkdir(parents=True)
    payload_path = payload_dir / f"{entry_index:04d}.decompressed.bin"
    payload_path.write_bytes(payload)
    digest = hashlib.sha256(payload).hexdigest()
    document = {
        "schema_version": 1,
        "data_tbl": {
            "sha256": "a" * 64,
            "entry_count": 926,
            "pack_count": 2,
        },
        "entries": [
            {
                "index": entry_index,
                "pac_name": "DATA00.PAC",
                "offset": 0x1000 * entry_index,
                "stored_size": len(payload),
                "expanded_size": len(payload),
                "decode": {"status": "decoded", "codec": "test"},
                "payload": {"size": len(payload), "sha256": digest},
                "payload_path": str(payload_path.relative_to(root)),
            }
        ],
    }
    manifest = root / "manifest.json"
    manifest.write_text(json.dumps(document), encoding="utf-8")
    return manifest


def snapshot(implementation: str, r3: int, value: float = 1.0) -> dict:
    return {
        "schema": "ac6.function-snapshot.v1",
        "identity": {
            "implementation": implementation,
            "function": "0x821A16B8",
            "case": "view-2-context-valid",
        },
        "exit": {"kind": "return"},
        "registers": {"r3": r3, "f1": value},
        "special_registers": {"cr": "0x00000000"},
        "calls": [{"target": "0x82200000", "ordinal": 0}],
        "memory_writes": [
            {"address": "0xA0000100", "size": 4, "after_hex": "00000001"}
        ],
    }


class ExtractPacTests(unittest.TestCase):
    def test_raw_entries_are_descrambled_in_decode_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset_root = root / "assets"
            output_root = root / "out"
            asset_root.mkdir()
            logical = make_fhm([b"NTXR" + b"\x01" * 12])
            stored = descramble(logical, 0)
            (asset_root / "DATA00.PAC").write_bytes(stored)
            (asset_root / "DATA01.PAC").write_bytes(b"")
            table = struct.pack(">II4I", 1, 2, 0x00020000, 0, len(stored), len(logical))
            (asset_root / "DATA.TBL").write_bytes(table)

            manifest = extract_selected(asset_root, output_root, [0], True)
            entry = manifest["entries"][0]
            self.assertEqual(entry["decode"]["codec"], "mode1_pi_xor_raw")
            self.assertEqual(entry["structure"]["root"], "FHM")
            self.assertEqual(
                (output_root / entry["payload_path"]).read_bytes(), logical
            )

    def test_raw_storage_can_be_preserved_explicitly(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset_root = root / "assets"
            output_root = root / "out"
            asset_root.mkdir()
            logical = b"ACE6raw-record"
            stored = descramble(logical, 0)
            (asset_root / "DATA00.PAC").write_bytes(stored)
            (asset_root / "DATA01.PAC").write_bytes(b"")
            (asset_root / "DATA.TBL").write_bytes(
                struct.pack(">II4I", 1, 2, 0x00020000, 0, len(stored), len(logical))
            )

            manifest = extract_selected(
                asset_root, output_root, [0], True, preserve_raw_storage=True
            )
            entry = manifest["entries"][0]
            self.assertEqual(entry["decode"]["status"], "preserved")
            self.assertEqual((output_root / entry["payload_path"]).read_bytes(), stored)

    def test_fhm_ignores_trailing_zero_size_capacity_slots(self) -> None:
        child = b"NDXR" + b"A" * 12
        # Preserve a valid child while expanding the table with the retail
        # sentinel used for unused FHM capacity slots.
        count = 4
        header_size = 0x14 + 8 * count
        child_offset = header_size
        expanded = bytearray(header_size + len(child))
        expanded[:4] = b"FHM "
        struct.pack_into(">I", expanded, 0x10, count)
        struct.pack_into(">I", expanded, 0x14, child_offset)
        sentinel_offset = len(expanded) + 7
        struct.pack_into(">I", expanded, 0x18, sentinel_offset)
        struct.pack_into(">I", expanded, 0x1c, sentinel_offset)
        struct.pack_into(">I", expanded, 0x20, sentinel_offset)
        sizes_begin = 0x14 + count * 4
        struct.pack_into(">I", expanded, sizes_begin, len(child))
        expanded[child_offset:] = child
        self.assertEqual(len(parse_fhm(bytes(expanded))), 1)

    def test_selected_entry_identity_survives_adjacent_indices(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset_root = root / "assets"
            output_root = root / "out"
            asset_root.mkdir()
            payload_119 = make_fhm([b"NDXR" + b"A" * 12])
            payload_120 = make_fhm([b"NTXR" + b"B" * 12])
            stored_119 = descramble(payload_119, 119)
            stored_120 = descramble(payload_120, 120)
            offset_119 = 0x1000
            offset_120 = offset_119 + len(stored_119)
            pac = bytearray(offset_120 + len(stored_120))
            pac[offset_119 : offset_119 + len(stored_119)] = stored_119
            pac[offset_120 : offset_120 + len(stored_120)] = stored_120
            (asset_root / "DATA00.PAC").write_bytes(pac)
            (asset_root / "DATA01.PAC").write_bytes(b"")
            entries = bytearray()
            for index in range(121):
                if index == 119:
                    entries += struct.pack(">4I", 0x00020000, offset_119,
                                           len(stored_119), len(payload_119))
                elif index == 120:
                    entries += struct.pack(">4I", 0x00020000, offset_120,
                                           len(stored_120), len(payload_120))
                else:
                    entries += struct.pack(">4I", 0x00020000, 0, 0, 0)
            (asset_root / "DATA.TBL").write_bytes(struct.pack(">II", 121, 2) + entries)

            manifest = extract_selected(asset_root, output_root, [119, 120], True)
            records = {entry["index"]: entry for entry in manifest["entries"]}
            self.assertEqual(set(records), {119, 120})
            self.assertEqual(records[119]["group_hex"], "0x00020000")
            self.assertEqual(records[120]["group_hex"], "0x00020000")
            self.assertEqual(records[119]["offset"], offset_119)
            self.assertEqual(records[120]["offset"], offset_120)
            closure_manifest = output_root / "manifest.json"
            closure = build_closure([closure_manifest], root / "closure")
            self.assertEqual([root["data_tbl_index"] for root in closure["roots"]], [119, 120])


class AssetClosureTests(unittest.TestCase):
    def test_closure_deduplicates_shared_content(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shared = b"NTXR" + b"A" * 12
            nested = make_fhm([shared, b"NDXR" + b"B" * 12])
            payload = make_fhm([shared, nested])
            manifest = write_extract_manifest(root / "extract", 9, payload)
            closure = build_closure([manifest], root / "closure")

            self.assertEqual(closure["stats"]["root_count"], 1)
            self.assertEqual(closure["stats"]["unique_node_count"], 4)
            self.assertEqual(closure["stats"]["occurrence_count"], 5)
            self.assertEqual(closure["stats"]["shared_node_count"], 1)
            shared_hash = hashlib.sha256(shared).hexdigest()
            node = next(node for node in closure["nodes"] if node["sha256"] == shared_hash)
            self.assertEqual(node["occurrence_count"], 2)
            self.assertTrue((root / "closure" / "occurrences.tsv").is_file())
            self.assertTrue((root / "closure" / "shared_nodes.tsv").is_file())

    def test_closure_comparison_reports_same_shape_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            left_payload = make_fhm([b"NDXR" + b"A" * 12])
            right_payload = make_fhm([b"NDXR" + b"B" * 12])
            left_manifest = write_extract_manifest(root / "left_extract", 119, left_payload)
            right_manifest = write_extract_manifest(root / "right_extract", 120, right_payload)
            build_closure([left_manifest], root / "left")
            build_closure([right_manifest], root / "right")

            report = compare_closures(
                root / "left" / "closure.json", root / "right" / "closure.json"
            )
            self.assertEqual(report["classification"], "different")
            self.assertGreater(report["stats"]["base_only_unique_nodes"], 0)
            signatures = {
                (record["magic"], record["size"])
                for record in report["changed_shape_signatures"]
            }
            self.assertIn(("NDXR", 16), signatures)


class FunctionSnapshotTests(unittest.TestCase):
    def test_generated_divergence_is_classified(self) -> None:
        report = compare_three(
            snapshot("ppc-pcode", 1),
            snapshot("xenonrecomp", 2),
            snapshot("native", 1),
        )
        self.assertEqual(report["classification"], "generated_diverges")
        self.assertFalse(report["equal"])
        self.assertTrue(report["pairs"]["ppc_native"]["equal"])

    def test_float_tolerance_can_be_explicit(self) -> None:
        report = compare_three(
            snapshot("ppc-pcode", 1, 1.0),
            snapshot("xenonrecomp", 1, 1.00001),
            snapshot("native", 1, 0.99999),
            abs_tol=0.0001,
        )
        self.assertEqual(report["classification"], "all_equal")


class NativeGateTests(unittest.TestCase):
    def test_j0_requires_native_evidence_and_can_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            test_log = root / "native-test.log"
            capture = root / "native-frame.ppm"
            test_log.write_text("native deterministic test passed\n", encoding="utf-8")
            capture.write_bytes(b"P6\n1 1\n255\n\x00\x00\x00")
            test_hash = hashlib.sha256(test_log.read_bytes()).hexdigest()
            capture_hash = hashlib.sha256(capture.read_bytes()).hexdigest()
            document = legacy_template()
            document["provenance"]["repo_commit"] = "1" * 40
            for name in (
                "native_session_loop",
                "gameplay_camera",
                "flight_input",
                "deterministic_replay",
            ):
                document["requirements"][name]["status"] = "passed"
                document["requirements"][name]["evidence"] = [
                    {
                        "kind": "native-test",
                        "path": test_log.name,
                        "sha256": test_hash,
                        "size": test_log.stat().st_size,
                        "claim": f"{name} passed in the native runtime",
                    }
                ]
            for name in ("world_visible", "player_aircraft_visible"):
                document["requirements"][name]["status"] = "passed"
                document["requirements"][name]["evidence"] = [
                    {
                        "kind": "native-capture",
                        "path": capture.name,
                        "sha256": capture_hash,
                        "size": capture.stat().st_size,
                        "claim": f"{name} appears in a native readback",
                    }
                ]
            report = audit_contract(document, root)
            self.assertTrue(report["J0"]["passed"])
            self.assertFalse(report["J1"]["passed"])

    def test_bridge_only_cannot_pass_a_native_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = root / "bridge.log"
            evidence.write_text("bridge observation\n", encoding="utf-8")
            document = legacy_template()
            document["provenance"]["repo_commit"] = "2" * 40
            record = document["requirements"]["native_session_loop"]
            record["status"] = "passed"
            record["evidence"] = [
                {
                    "kind": "bridge",
                    "path": evidence.name,
                    "sha256": hashlib.sha256(evidence.read_bytes()).hexdigest(),
                    "claim": "bridge loop runs",
                }
            ]
            with self.assertRaises(GateError):
                audit_contract(document, root)

    def test_v1_is_readable_but_never_promotes_retail(self) -> None:
        document = legacy_template()
        document["provenance"]["repo_commit"] = "3" * 40
        report = audit_contract(document, Path(tempfile.mkdtemp()))
        self.assertFalse(report["J1"]["passed"])
        self.assertFalse(report["retail_semantics_qualified"])

    def test_native_as_retail_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "native.json"
            fixture.write_text('{"retail_semantics_qualified": false}', encoding="utf-8")
            document = template()
            document["provenance"]["repo_commit"] = "4" * 40
            record = document["requirements"]["domains"]["retail_units_and_waves"]
            record["status"] = "passed"
            record["retail_semantics_qualified"] = False
            record["evidence"] = [{"kind": "native-test", "path": fixture.name, "sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(), "claim": "native fixture"}]
            with self.assertRaises(GateError):
                audit_contract(document, root)

    def test_bridge_only_and_fhm_colocation_do_not_qualify_retail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "fhm.json"
            fixture.write_text("bridge and co-located FHM", encoding="utf-8")
            document = template()
            document["provenance"]["repo_commit"] = "5" * 40
            record = document["requirements"]["domains"]["retail_objectives"]
            record["status"] = "passed"
            record["retail_semantics_qualified"] = True
            record["evidence"] = [{"kind": "bridge", "path": fixture.name, "sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(), "claim": "FHM co-location only"}]
            with self.assertRaises(GateError):
                audit_contract(document, root)

    def test_evidence_provenance_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "native.log"
            fixture.write_text("qualified native evidence\n", encoding="utf-8")
            document = template()
            document["provenance"]["repo_commit"] = "6" * 40
            record = document["requirements"]["J0"]["native_session_loop"]
            record["status"] = "passed"
            record["evidence"] = [{
                "kind": "native-test",
                "path": fixture.name,
                "sha256": "0" * 64,
                "size": fixture.stat().st_size,
                "claim": "native evidence with mismatched provenance",
            }]
            with self.assertRaises(GateError):
                audit_contract(document, root)

    def test_retail_provenance_mismatch_is_rejected(self) -> None:
        document = template()
        document["provenance"]["repo_commit"] = "7" * 40
        document["provenance"]["xex_sha256"] = "0" * 64
        with self.assertRaises(GateError):
            audit_contract(document, Path(tempfile.mkdtemp()))

    def test_v2_template_requires_native_wave_mechanics(self) -> None:
        document = template()
        self.assertIn(
            "native_units_and_waves",
            document["policy"]["retail_gate_requires"],
        )

    def test_v2_j0_static_evidence_cannot_replace_native_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = root / "static.json"
            evidence.write_text("{}", encoding="utf-8")
            document = template()
            document["provenance"]["repo_commit"] = "8" * 40
            record = document["requirements"]["J0"]["world_visible"]
            record["status"] = "passed"
            record["evidence"] = [{
                "kind": "static",
                "path": evidence.name,
                "sha256": hashlib.sha256(evidence.read_bytes()).hexdigest(),
                "claim": "static geometry exists",
            }]
            with self.assertRaises(GateError):
                audit_contract(document, root)

    def test_v2_retail_gate_requires_native_consumption(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence = root / "retail.json"
            evidence.write_text('{"retail_semantics_qualified": true}', encoding="utf-8")
            document = template()
            document["provenance"]["repo_commit"] = "9" * 40
            record = document["requirements"]["domains"]["retail_objectives"]
            record["status"] = "passed"
            record["retail_semantics_qualified"] = True
            record["evidence"] = [{
                "kind": "static",
                "path": evidence.name,
                "sha256": hashlib.sha256(evidence.read_bytes()).hexdigest(),
                "claim": "retail objective record identified statically",
            }]
            with self.assertRaises(GateError):
                audit_contract(document, root)

    def test_v2_policy_drift_is_rejected(self) -> None:
        document = template()
        document["provenance"]["repo_commit"] = "a" * 40
        document["policy"]["retail_gate_requires"].remove("native_units_and_waves")
        with self.assertRaises(GateError):
            audit_contract(document, Path(tempfile.mkdtemp()))


if __name__ == "__main__":
    unittest.main()
