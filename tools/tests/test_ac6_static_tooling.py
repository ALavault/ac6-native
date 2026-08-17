from __future__ import annotations

import hashlib
import copy
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import map_object_layout
import inventory_ac6_pac_shaders
from ac6_mode1_codec import descramble
from ac6_fhm import parse_fhm
from audit_ac6_mission01_native_gate import GateError, audit_contract, template
from build_ac6_asset_closure import build_closure
from compare_ac6_asset_closures import compare as compare_closures
from compare_ac6_function_snapshots import compare_three
from extract_ac6_pac import extract_selected
from validate_ac6_scenario_schema import main as validate_scenario_schema
from emit_ac6_reader_digests import canonical as digest_canonical, fnv64 as digest_fnv64
from roundtrip_ac6_scenario import Walk as ScenarioWalk, reemit as scenario_reemit
from emit_mission01_retail_manifests import (CLASS_TO_OBJECT_CATEGORY, ENTITY_BASE,
                                             objectives_rows, waves_rows)
from emit_ac6_native_snapshot import (Image as NativeImage, Parsers as NativeParsers,
                                      Payload as NativePayload,
                                      RECORD_BASE as NativeRecordBase,
                                      BUFFER_BASE as NativeBufferBase)


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
    def test_fhm_accepts_minimal_empty_container(self) -> None:
        empty = bytearray(0x14)
        empty[:4] = b"FHM "
        empty[4:8] = b"\x01\x01\x00\x10"
        self.assertEqual(parse_fhm(bytes(empty)), [])

    def test_pac_shader_inventory_hashes_both_dword_orders(self) -> None:
        microcode = bytes.fromhex("0102030411223344aabbccdd")
        container = bytearray(0x4c)
        struct.pack_into(">III", container, 0, 0x102A1101, 0x40, len(microcode))
        struct.pack_into(">I", container, 0x18, 0x20)
        struct.pack_into(">II", container, 0x20, 0, len(microcode))
        container[0x40:] = microcode
        nsxr = bytearray(0x80)
        nsxr[:4] = b"NSXR"
        struct.pack_into(">I", nsxr, 4, len(nsxr))
        nsxr[0x20 : 0x20 + len(container)] = container
        records = inventory_ac6_pac_shaders.scan_nsxr_leaf(
            bytes(nsxr), entry_index=7, path="0007/0000"
        )
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]["sha256_raw"], hashlib.sha256(microcode).hexdigest())
        self.assertEqual(
            records[0]["sha256_swap32"],
            hashlib.sha256(bytes.fromhex("0403020144332211ddccbbaa")).hexdigest(),
        )

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

    def test_data01_uses_archive_local_codec_index(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset_root = root / "assets"
            output_root = root / "out"
            asset_root.mkdir()
            data00_payload = make_fhm([b"NDXR" + b"A" * 12])
            data01_payload = make_fhm([b"NTXR" + b"B" * 12])
            compressor = zlib.compressobj(level=9, wbits=-15)
            compressed = compressor.compress(data01_payload) + compressor.flush()
            data00_stored = descramble(data00_payload, 0)
            data01_stored = descramble(compressed, 0)
            (asset_root / "DATA00.PAC").write_bytes(data00_stored)
            (asset_root / "DATA01.PAC").write_bytes(data01_stored)
            table = struct.pack(
                ">II4I4I",
                2,
                2,
                0x00020000,
                0,
                len(data00_stored),
                len(data00_payload),
                0x01010000,
                0,
                len(data01_stored),
                len(data01_payload),
            )
            (asset_root / "DATA.TBL").write_bytes(table)

            manifest = extract_selected(asset_root, output_root, [0, 1], True)
            records = {entry["index"]: entry for entry in manifest["entries"]}
            self.assertEqual(records[0]["codec_index"], 0)
            self.assertEqual(records[1]["codec_index"], 0)
            self.assertEqual(records[1]["decode"]["codec_index"], 0)
            self.assertEqual(
                (output_root / records[1]["payload_path"]).read_bytes(),
                data01_payload,
            )

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


class ScenarioSchemaTests(unittest.TestCase):
    """Cover the schema validator with synthetic nodes, no retail bytes."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _payload(self, obj_children: int, param_tag: int) -> bytes:
        """Build one scenario root reaching a single ObjBin node.

        Layout is flat and hand-placed so every offset in the file is a
        deliberate value the test can reason about.
        """
        blob = bytearray(0x400)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        # The Obj node's children: child[0] is the param variant, the rest are
        # plain present nodes pointing at themselves.
        child_base = 0x300
        for index in range(obj_children):
            here = child_base + 0x10 * index
            # data_off resolves to the tag byte for child[0], to itself otherwise
            put(here, self._node(0x08, 0x00))
            put(here + 0x08, bytes([param_tag if index == 0 else 0, 0, 0, 0]))

        obj_table = 0x280
        put(obj_table, self._table([child_base - obj_table + 0x10 * i
                                    for i in range(obj_children)]))
        obj_node = 0x260
        put(obj_node, self._node(0x08, obj_table - obj_node))
        put(obj_node + 0x08, b"\x01\x00\x00\x00")

        # 0x8232F198 level: child[0] is the Obj node.
        inner_table = 0x240
        put(inner_table, self._table([obj_node - inner_table]))
        inner = 0x230
        put(inner, self._node(0x08, inner_table - inner))
        put(inner + 0x08, b"\x01\x00\x00\x00")

        # 0x8232F380 level: u8 count in data, one child.
        array_table = 0x200
        put(array_table, self._table([inner - array_table]))
        array_node = 0x1F0
        put(array_node, self._node(0x08, array_table - array_node))
        put(array_node + 0x08, bytes([1, 0, 0, 0]))

        # 0x8232CCA0 level: two slots, slot 1 is the array node.
        dispatch_table = 0x1C0
        put(dispatch_table, self._table([array_node - dispatch_table,
                                         array_node - dispatch_table]))
        dispatch = 0x1B0
        put(dispatch, self._node(0x08, dispatch_table - dispatch))
        put(dispatch + 0x08, b"\x01\x00\x00\x00")

        entry_table = 0x1A0
        put(entry_table, self._table([dispatch - entry_table]))
        entry = 0x190
        put(entry, self._node(0x08, entry_table - entry))
        put(entry + 0x08, b"\x01\x00\x00\x00")

        slot0_table = 0x180
        put(slot0_table, self._table([entry - slot0_table]))
        slot0 = 0x170
        put(slot0, self._node(0x08, slot0_table - slot0))
        put(slot0 + 0x08, b"\x01\x00\x00\x00")

        root_table = 0x160
        put(root_table, self._table([slot0 - root_table]))
        put(0, self._node(0x08, root_table))
        put(0x08, b"\x01\x00\x00\x00")
        return bytes(blob)

    def _schema(self, payload: bytes, **validation) -> dict:
        return {
            "schema": "ac6.scenario-schema.v1",
            "class": "ObjBin",
            "record": {"fields": [{"name": name} for name in
                                  ("data", "param", "maneuvers", "durable",
                                   "weapon_0", "weapon_1", "weapon_2", "tail")]},
            "validation": {"payload_sha256": hashlib.sha256(payload).hexdigest(),
                           **validation},
        }

    def _run(self, schema: dict, payload: bytes) -> int:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema_path = root / "schema.json"
            payload_path = root / "payload.bin"
            schema_path.write_text(json.dumps(schema))
            payload_path.write_bytes(payload)
            return validate_scenario_schema([str(schema_path), str(payload_path),
                                             "--output", str(root / "out.json")])

    def test_seven_children_and_tag_in_range_pass(self):
        payload = self._payload(obj_children=7, param_tag=1)
        schema = self._schema(payload, slot0_entries=1, obj_records_reached=1,
                              inconsistencies=0)
        self.assertEqual(self._run(schema, payload), 0)

    def test_more_children_than_the_schema_allows_fails(self):
        payload = self._payload(obj_children=9, param_tag=0)
        schema = self._schema(payload)
        self.assertEqual(self._run(schema, payload), 1)

    def test_param_tag_outside_the_reader_range_fails(self):
        payload = self._payload(obj_children=7, param_tag=5)
        schema = self._schema(payload)
        self.assertEqual(self._run(schema, payload), 1)

    def test_payload_hash_mismatch_fails_closed(self):
        payload = self._payload(obj_children=7, param_tag=0)
        schema = self._schema(payload)
        schema["validation"]["payload_sha256"] = "0" * 64
        self.assertEqual(self._run(schema, payload), 2)

    def test_recorded_counters_are_cross_checked(self):
        payload = self._payload(obj_children=7, param_tag=0)
        # The walk reaches one record; claiming two must fail rather than pass.
        schema = self._schema(payload, obj_records_reached=2)
        self.assertEqual(self._run(schema, payload), 1)


class OrderSchemaTests(unittest.TestCase):
    """Cover the OrderBin walk with synthetic Set/Act/Order nodes."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _payload(self, order_tag: int) -> bytes:
        """One Set holding one Act holding one Order with the given tag."""
        blob = bytearray(0x400)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        # Order node: data carries the tag, one child.
        order_child = 0x340
        put(order_child, self._node(0x08, 0x00))
        put(order_child + 0x08, b"\x01\x00\x00\x00")
        order_table = 0x330
        put(order_table, self._table([order_child - order_table]))
        order = 0x320
        put(order, self._node(0x08, order_table - order))
        put(order + 0x08, bytes([order_tag, 0, 0, 0]))

        # Act node: u8 order count in data, one child.
        act_table = 0x300
        put(act_table, self._table([order - act_table]))
        act = 0x2F0
        put(act, self._node(0x08, act_table - act))
        put(act + 0x08, bytes([1, 0, 0, 0]))

        # Set node: u8 act count in data, one child.
        set_table = 0x2D0
        put(set_table, self._table([act - set_table]))
        set_node = 0x2C0
        put(set_node, self._node(0x08, set_table - set_node))
        put(set_node + 0x08, bytes([1, 0, 0, 0]))

        # 0x8232CCA0 dispatch node: slot 0 is the Set, slot 1 an empty array.
        empty_array = 0x2A0
        put(empty_array, self._node(0x08, 0x00))
        put(empty_array + 0x08, bytes([0, 0, 0, 0]))
        dispatch_table = 0x280
        put(dispatch_table, self._table([set_node - dispatch_table,
                                         empty_array - dispatch_table]))
        dispatch = 0x270
        put(dispatch, self._node(0x08, dispatch_table - dispatch))
        put(dispatch + 0x08, b"\x01\x00\x00\x00")

        entry_table = 0x260
        put(entry_table, self._table([dispatch - entry_table]))
        entry = 0x250
        put(entry, self._node(0x08, entry_table - entry))
        put(entry + 0x08, b"\x01\x00\x00\x00")

        slot0_table = 0x240
        put(slot0_table, self._table([entry - slot0_table]))
        slot0 = 0x230
        put(slot0, self._node(0x08, slot0_table - slot0))
        put(slot0 + 0x08, b"\x01\x00\x00\x00")

        root_table = 0x220
        put(root_table, self._table([slot0 - root_table]))
        put(0, self._node(0x08, root_table))
        put(0x08, b"\x01\x00\x00\x00")
        return bytes(blob)

    def _schema(self, payload: bytes, **validation) -> dict:
        return {
            "schema": "ac6.scenario-schema.v1",
            "class": "OrderBin",
            "record": {"size": "0x2C"},
            "validation": {"payload_sha256": hashlib.sha256(payload).hexdigest(),
                           **validation},
        }

    def _run(self, schema: dict, payload: bytes) -> int:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema_path = root / "schema.json"
            payload_path = root / "payload.bin"
            schema_path.write_text(json.dumps(schema))
            payload_path.write_bytes(payload)
            return validate_scenario_schema([str(schema_path), str(payload_path),
                                             "--output", str(root / "out.json")])

    def test_tag_inside_the_reader_range_passes(self):
        payload = self._payload(order_tag=5)
        schema = self._schema(payload, set_nodes=1, act_nodes=1,
                              order_records_reached=1, inconsistencies=0,
                              tag_distribution={"5": 1})
        self.assertEqual(self._run(schema, payload), 0)

    def test_tag_outside_the_reader_range_fails(self):
        payload = self._payload(order_tag=10)
        self.assertEqual(self._run(self._schema(payload), payload), 1)

    def test_recorded_tag_distribution_is_cross_checked(self):
        payload = self._payload(order_tag=3)
        schema = self._schema(payload, tag_distribution={"8": 1})
        self.assertEqual(self._run(schema, payload), 1)


class ActSchemaTests(unittest.TestCase):
    """Cover the ActBin list-header rules with synthetic Set/Act nodes."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _payload(self, declared_orders: int, real_orders: int) -> bytes:
        """One Set, one Act declaring `declared_orders` over `real_orders` children."""
        blob = bytearray(0x500)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        order_base = 0x400
        for index in range(real_orders):
            here = order_base + 0x20 * index
            child = here + 0x10
            put(child, self._node(0x08, 0x00))
            put(child + 0x08, b"\x01\x00\x00\x00")
            put(here, self._node(0x08, 0x00))
            put(here + 0x08, bytes([5, 0, 0, 0]))   # a tag the reader handles

        act_table = 0x3A0
        put(act_table, self._table([order_base - act_table + 0x20 * i
                                    for i in range(real_orders)]))
        act = 0x390
        put(act, self._node(0x08, act_table - act))
        put(act + 0x08, bytes([declared_orders, 0, 0, 0]))

        set_table = 0x370
        put(set_table, self._table([act - set_table]))
        set_node = 0x360
        put(set_node, self._node(0x08, set_table - set_node))
        put(set_node + 0x08, b"\x01\x00\x00\x00")

        empty_array = 0x340
        put(empty_array, self._node(0x08, 0x00))
        put(empty_array + 0x08, bytes([0, 0, 0, 0]))
        dispatch_table = 0x320
        put(dispatch_table, self._table([set_node - dispatch_table,
                                         empty_array - dispatch_table]))
        dispatch = 0x310
        put(dispatch, self._node(0x08, dispatch_table - dispatch))
        put(dispatch + 0x08, b"\x01\x00\x00\x00")

        entry_table = 0x300
        put(entry_table, self._table([dispatch - entry_table]))
        entry = 0x2F0
        put(entry, self._node(0x08, entry_table - entry))
        put(entry + 0x08, b"\x01\x00\x00\x00")

        slot0_table = 0x2E0
        put(slot0_table, self._table([entry - slot0_table]))
        slot0 = 0x2D0
        put(slot0, self._node(0x08, slot0_table - slot0))
        put(slot0 + 0x08, b"\x01\x00\x00\x00")

        root_table = 0x2C0
        put(root_table, self._table([slot0 - root_table]))
        put(0, self._node(0x08, root_table))
        put(0x08, b"\x01\x00\x00\x00")
        return bytes(blob)

    def _schema(self, payload: bytes, **validation) -> dict:
        return {
            "schema": "ac6.scenario-schema.v1",
            "class": "ActBin",
            "record": {"size": "0x08"},
            "validation": {"payload_sha256": hashlib.sha256(payload).hexdigest(),
                           **validation},
        }

    def _run(self, schema: dict, payload: bytes) -> int:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema_path = root / "schema.json"
            payload_path = root / "payload.bin"
            schema_path.write_text(json.dumps(schema))
            payload_path.write_bytes(payload)
            return validate_scenario_schema([str(schema_path), str(payload_path),
                                             "--output", str(root / "out.json")])

    def test_declared_count_matching_the_table_passes(self):
        payload = self._payload(declared_orders=3, real_orders=3)
        schema = self._schema(payload, set_nodes=1, act_nodes=1,
                              inconsistencies=0, act_count_overruns=0,
                              orders_per_act={"3": 1})
        self.assertEqual(self._run(schema, payload), 0)

    def test_declared_count_overrunning_the_table_fails(self):
        payload = self._payload(declared_orders=5, real_orders=2)
        self.assertEqual(self._run(self._schema(payload), payload), 1)

    def test_act_declaring_zero_orders_is_counted_not_walked(self):
        payload = self._payload(declared_orders=0, real_orders=2)
        schema = self._schema(payload, acts_declaring_zero_orders=1,
                              act_count_overruns=0, inconsistencies=0)
        self.assertEqual(self._run(schema, payload), 0)


class ManeuverSchemaTests(unittest.TestCase):
    """Cover the ManeuverBin s32 count and its two-hop descent to ComTblBin."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _payload(self, declared: int, real: int, break_descent: bool = False) -> bytes:
        """One Obj whose maneuver block holds one ManeuverBin with `real` elements."""
        blob = bytearray(0x800)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        # Each maneuver element: data is the ComTblM pointer, table[0] leads to
        # a node whose own table[0] is the ComTblBin node.
        element_base = 0x600
        for index in range(real):
            here = element_base + 0x40 * index
            comtbl = here + 0x30
            put(comtbl, self._node(0x08, 0x00))
            put(comtbl + 0x08, bytes([0, 0, 0, 0]))   # zero coms: the early exit
            inner_table = here + 0x20
            put(inner_table, self._table([] if break_descent else [comtbl - inner_table]))
            put(here, self._node(0x08, inner_table - here))
            put(here + 0x08, b"\x01\x00\x00\x00")

        man_table = 0x560
        put(man_table, self._table([element_base - man_table + 0x40 * i
                                    for i in range(real)]))
        maneuver = 0x550
        put(maneuver, self._node(0x08, man_table - maneuver))
        put(maneuver + 0x08, struct.pack(">i", declared))

        block_table = 0x530
        put(block_table, self._table([maneuver - block_table]))
        block = 0x520
        put(block, self._node(0x08, block_table - block))
        put(block + 0x08, b"\x01\x00\x00\x00")

        # Obj node needs child[1] to be the maneuver block.
        obj_table = 0x500
        put(obj_table, self._table([block - obj_table, block - obj_table]))
        obj = 0x4F0
        put(obj, self._node(0x08, obj_table - obj))
        put(obj + 0x08, b"\x01\x00\x00\x00")

        inner_t = 0x4E0
        put(inner_t, self._table([obj - inner_t]))
        inner = 0x4D0
        put(inner, self._node(0x08, inner_t - inner))
        put(inner + 0x08, b"\x01\x00\x00\x00")

        array_table = 0x4B0
        put(array_table, self._table([inner - array_table]))
        array_node = 0x4A0
        put(array_node, self._node(0x08, array_table - array_node))
        put(array_node + 0x08, bytes([1, 0, 0, 0]))

        dispatch_table = 0x480
        put(dispatch_table, self._table([array_node - dispatch_table,
                                         array_node - dispatch_table]))
        dispatch = 0x470
        put(dispatch, self._node(0x08, dispatch_table - dispatch))
        put(dispatch + 0x08, b"\x01\x00\x00\x00")

        entry_table = 0x460
        put(entry_table, self._table([dispatch - entry_table]))
        entry = 0x450
        put(entry, self._node(0x08, entry_table - entry))
        put(entry + 0x08, b"\x01\x00\x00\x00")

        slot0_table = 0x440
        put(slot0_table, self._table([entry - slot0_table]))
        slot0 = 0x430
        put(slot0, self._node(0x08, slot0_table - slot0))
        put(slot0 + 0x08, b"\x01\x00\x00\x00")

        root_table = 0x420
        put(root_table, self._table([slot0 - root_table]))
        put(0, self._node(0x08, root_table))
        put(0x08, b"\x01\x00\x00\x00")
        return bytes(blob)

    def _schema(self, payload: bytes, **validation) -> dict:
        return {
            "schema": "ac6.scenario-schema.v1",
            "class": "ManeuverBin",
            "record": {"size": "0x0C"},
            "validation": {"payload_sha256": hashlib.sha256(payload).hexdigest(),
                           **validation},
        }

    def _run(self, schema: dict, payload: bytes) -> int:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema_path = root / "schema.json"
            payload_path = root / "payload.bin"
            schema_path.write_text(json.dumps(schema))
            payload_path.write_bytes(payload)
            return validate_scenario_schema([str(schema_path), str(payload_path),
                                             "--output", str(root / "out.json")])

    def test_full_descent_to_comtbl_passes(self):
        payload = self._payload(declared=2, real=2)
        schema = self._schema(payload, maneuver_nodes=1, maneuver_elements=2,
                              inconsistencies=0,
                              element_descent={"reaches_comtbl": 2})
        self.assertEqual(self._run(schema, payload), 0)

    def test_declared_count_overrunning_the_table_fails(self):
        payload = self._payload(declared=5, real=2)
        self.assertEqual(self._run(self._schema(payload), payload), 1)

    def test_broken_descent_is_classified_not_silently_dropped(self):
        payload = self._payload(declared=2, real=2, break_descent=True)
        schema = self._schema(payload, maneuver_elements=2, inconsistencies=0,
                              element_descent={"comtblm_child_absent": 2})
        self.assertEqual(self._run(schema, payload), 0)


class ListHeaderSchemaTests(unittest.TestCase):
    """Cover the schema-driven list-header walk across the three count widths."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _payload(self, slot: int, count_bytes: bytes, real: int) -> bytes:
        """Root with `slot+1` slots; the chosen slot is a list header."""
        blob = bytearray(0x400)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        element_base = 0x300
        for index in range(real):
            here = element_base + 0x10 * index
            put(here, self._node(0x08, 0x00))
            put(here + 0x08, b"\x01\x00\x00\x00")

        header_table = 0x2C0
        put(header_table, self._table([element_base - header_table + 0x10 * i
                                       for i in range(real)]))
        header = 0x2B0
        put(header, self._node(0x08, header_table - header))
        put(header + 0x08, count_bytes)

        root_table = 0x200
        put(root_table, self._table([header - root_table] * (slot + 1)))
        put(0, self._node(0x08, root_table))
        put(0x08, b"\x01\x00\x00\x00")
        return bytes(blob)

    def _schema(self, payload: bytes, width: str, stride: int, slot: int,
                **validation) -> dict:
        return {
            "schema": "ac6.scenario-schema.v1",
            "class": "SyntheticListHeader",
            "list_header": {"count_type": width, "count_offset": 0,
                            "element_stride": stride, "element": "Synthetic"},
            "reach": [{"op": "root"}, {"op": "child", "index": slot}],
            "validation": {"payload_sha256": hashlib.sha256(payload).hexdigest(),
                           **validation},
        }

    def _run(self, schema: dict, payload: bytes) -> int:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema_path = root / "schema.json"
            payload_path = root / "payload.bin"
            schema_path.write_text(json.dumps(schema))
            payload_path.write_bytes(payload)
            return validate_scenario_schema([str(schema_path), str(payload_path),
                                             "--output", str(root / "out.json")])

    def test_u8_count(self):
        payload = self._payload(0, bytes([3, 0, 0, 0]), 3)
        schema = self._schema(payload, "u8", 16, 0, declared_count=3,
                              elements_present=3, inconsistencies=0)
        self.assertEqual(self._run(schema, payload), 0)

    def test_u16_count(self):
        payload = self._payload(3, struct.pack(">HH", 5, 0), 5)
        schema = self._schema(payload, "u16", 16, 3, declared_count=5,
                              elements_present=5, inconsistencies=0)
        self.assertEqual(self._run(schema, payload), 0)

    def test_s32_count(self):
        payload = self._payload(1, struct.pack(">i", 2), 2)
        schema = self._schema(payload, "s32", 12, 1, declared_count=2,
                              elements_present=2, inconsistencies=0)
        self.assertEqual(self._run(schema, payload), 0)

    def test_count_overrunning_the_table_fails(self):
        payload = self._payload(0, bytes([9, 0, 0, 0]), 2)
        self.assertEqual(self._run(self._schema(payload, "u8", 16, 0), payload), 1)

    def test_reading_the_wrong_count_width_is_visible(self):
        # A u16 count of 5 read as u8 yields 0, so the declared count changes.
        payload = self._payload(0, struct.pack(">HH", 5, 0), 5)
        schema = self._schema(payload, "u8", 16, 0, declared_count=5)
        self.assertEqual(self._run(schema, payload), 1)


class NativeSnapshotTests(unittest.TestCase):
    """Guard the reader/sizer asymmetries a single sample would have hidden."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _block(self, present_slots: list[int]) -> tuple[NativePayload, int]:
        """A maneuver block whose slots in `present_slots` are non-empty."""
        blob = bytearray(0x400)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        slot_base = 0x200
        offsets = []
        for index in range(8):
            here = slot_base + 0x40 * index
            if index in present_slots:
                # A ManeuverBin node declaring zero elements: size is 0.
                put(here, self._node(0x08, 0x00))
                put(here + 0x08, struct.pack(">i", 0))
            # absent slots stay {0, 0}
            offsets.append(here)

        table = 0x180
        put(table, self._table([offset - table for offset in offsets]))
        block = 0x170
        put(block, self._node(0x08, table - block))
        put(block + 0x08, b"\x01\x00\x00\x00")
        return NativePayload(bytes(blob)), block

    def _size(self, present_slots: list[int]) -> int:
        payload, block = self._block(present_slots)
        parsers = NativeParsers(payload, NativeImage())
        return parsers.maneuver_block_size(block)

    def test_empty_block_reserves_the_fixed_base(self):
        # 0x82330A30 starts at 0x60 - eight maneuver records of 0x0C.
        self.assertEqual(self._size([]), 0x60)

    def test_slot_zero_overwrites_the_base_rather_than_adding(self):
        # size0 is 0 here, so the total is exactly 0x6C, not 0x60 + 0x0C + 0.
        self.assertEqual(self._size([0]), 0x6C)

    def test_later_slots_add_to_whichever_base_applies(self):
        self.assertEqual(self._size([1]), 0x60 + 0x0C)
        self.assertEqual(self._size([0, 1]), 0x6C + 0x0C)

    def test_the_asymmetry_is_exactly_0x60(self):
        # The reader advances 0x0C per present slot; the sizer, which is what
        # ObjBin::read follows, differs by 0x60 once slot 0 is present.
        reader_style = 0x0C * 2
        self.assertEqual(self._size([0, 1]) - reader_style, 0x60)


class OrderNativeParserTests(unittest.TestCase):
    """Guard the OrderBin tag dispatch and the exact-write mask."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _order(self, tag: int, child_present: bool = True) -> tuple[NativePayload, int]:
        blob = bytearray(0x200)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        child = 0x100
        if child_present:
            put(child, self._node(0x08, 0x00))
            put(child + 0x08, b"\x01\x00\x00\x00")
        table = 0x0C0
        put(table, self._table([child - table]))
        node = 0x0B0
        put(node, self._node(0x08, table - node))
        put(node + 0x08, bytes([tag, 0, 0, 0]))
        return NativePayload(bytes(blob)), node

    def _run(self, tag: int, child_present: bool = True):
        payload, node = self._order(tag, child_present)
        image = NativeImage()
        parsers = NativeParsers(payload, image)
        parsers.order_read(NativeRecordBase, node, NativeBufferBase)
        return image, parsers

    def test_each_tag_writes_its_own_record_slot(self):
        for tag in range(10):
            with self.subTest(tag=tag):
                image, _ = self._run(tag)
                offsets = set()
                for run in image.runs():
                    base = int(run["address"], 16)
                    if base < NativeRecordBase + 0x100:
                        start = base - NativeRecordBase
                        offsets.update(start + 4 * k for k in range(run["size"] // 4))
                self.assertIn(4 * (tag + 1), offsets)

    def test_a_tag_outside_the_reader_range_writes_only_the_data_word(self):
        image, _ = self._run(11)
        offsets = [run["address"] for run in image.runs()]
        self.assertEqual(offsets, [f"0x{NativeRecordBase:08x}"])

    def test_named_variants_fail_closed_on_an_absent_child(self):
        # Tag 3 is OrderStopBin, which has a failure path; tag 0 has none.
        _, parsers = self._run(3, child_present=False)
        self.assertEqual(len(parsers.errors), 1)
        self.assertIn("OrderStopBin", parsers.errors[0]["message"])
        _, quiet = self._run(0, child_present=False)
        self.assertEqual(quiet.errors, [])

    def test_a_written_byte_equal_to_the_poison_is_still_reported(self):
        # The write mask must not lose a byte that happens to equal 0xCD.
        image = NativeImage()
        image.write32(NativeBufferBase, 0xCDCDCDCD)
        self.assertEqual(image.runs(),
                         [{"address": f"0x{NativeBufferBase:08x}", "size": 4,
                           "after_hex": "cdcdcdcd"}])


class SetActNativeParserTests(unittest.TestCase):
    """Guard the two list-header readers and their early exit."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _act(self, declared: int, real: int) -> tuple[NativePayload, int]:
        """One Act over `real` orders, declaring `declared` of them."""
        blob = bytearray(0x400)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        base = 0x200
        for index in range(real):
            here = base + 0x40 * index
            child = here + 0x20
            put(child, self._node(0x08, 0x00))
            put(child + 0x08, b"\x01\x00\x00\x00")
            order_table = here + 0x10
            put(order_table, self._table([child - order_table]))
            put(here, self._node(0x08, order_table - here))
            put(here + 0x08, bytes([5, 0, 0, 0]))   # OrderJumpBin

        table = 0x180
        put(table, self._table([base - table + 0x40 * i for i in range(real)]))
        act = 0x170
        put(act, self._node(0x08, table - act))
        put(act + 0x08, bytes([declared, 0, 0, 0]))
        return NativePayload(bytes(blob)), act

    def _run_act(self, declared: int, real: int):
        payload, act = self._act(declared, real)
        image = NativeImage()
        parsers = NativeParsers(payload, image)
        parsers.act_read(NativeRecordBase, act, NativeBufferBase)
        return image, parsers

    def test_orders_are_laid_out_at_the_0x2c_stride(self):
        image, _ = self._run_act(declared=3, real=3)
        buffer_runs = [run for run in image.runs()
                       if int(run["address"], 16) >= NativeBufferBase]
        # Three zeroed 0x2C records are contiguous: 3 * 0x2C = 0x84 bytes.
        self.assertEqual(buffer_runs[0]["address"], f"0x{NativeBufferBase:08x}")
        self.assertGreaterEqual(buffer_runs[0]["size"], 3 * 0x2C)

    def test_zero_orders_takes_the_early_exit(self):
        image, parsers = self._run_act(declared=0, real=2)
        # Only the data word is written: no orders pointer, no element array.
        self.assertEqual([run["address"] for run in image.runs()],
                         [f"0x{NativeRecordBase:08x}"])
        self.assertEqual(parsers.errors, [])

    def test_declaring_more_orders_than_the_table_fails_closed(self):
        _, parsers = self._run_act(declared=3, real=1)
        messages = [error["message"] for error in parsers.errors]
        self.assertEqual(len(messages), 2)
        self.assertTrue(all("order empty" in message for message in messages))

    def test_act_size_matches_the_records_plus_their_orders(self):
        payload, act = self._act(declared=2, real=2)
        parsers = NativeParsers(payload, NativeImage())
        # Two 0x2C records plus two tag-5 orders of 4 bytes each.
        self.assertEqual(parsers.act_size(act), 2 * 0x2C + 2 * 4)


class SizerQuirkTests(unittest.TestCase):
    """Guard the four reader/sizer quirks breadth testing exposed."""

    @staticmethod
    def _node(data_off: int, table_off: int) -> bytes:
        return struct.pack(">II", data_off, table_off)

    @staticmethod
    def _table(child_offsets: list[int]) -> bytes:
        body = struct.pack(">i", len(child_offsets))
        for offset in child_offsets:
            body += struct.pack(">I", offset)
        return body

    def _list(self, elements: int, tag: int = 0) -> tuple[NativePayload, int]:
        """An unnamed 0x28 list with `elements` entries of the given tag."""
        blob = bytearray(0x600)

        def put(offset: int, data: bytes) -> None:
            blob[offset:offset + len(data)] = data

        base = 0x300
        for index in range(elements):
            here = base + 0x60 * index
            grand = here + 0x40
            put(grand, self._node(0x08, 0x00))
            put(grand + 0x08, b"\x01\x00\x00\x00")
            inner = here + 0x20
            put(inner, self._table([grand - inner]))
            put(here, self._node(0x08, inner - here))
            put(here + 0x08, bytes([tag, 0, 0, 0]))

        table = 0x280
        put(table, self._table([base - table + 0x60 * i for i in range(elements)]))
        node = 0x270
        put(node, self._node(0x08, table - node))
        put(node + 0x08, bytes([elements, 0, 0, 0]))
        return NativePayload(bytes(blob)), node

    def _size(self, elements: int) -> int:
        payload, node = self._list(elements)
        return NativeParsers(payload, NativeImage()).unnamed28_list_size(node)

    def test_the_list_sizer_rounds_up_to_sixteen(self):
        # Two elements: 2*0x28 + 2*4 = 88, which rounds to 96.
        self.assertEqual(self._size(2), 96)
        # One element: 0x28 + 4 = 44, which rounds to 48.
        self.assertEqual(self._size(1), 48)

    def test_the_round_up_is_the_only_difference_from_the_raw_total(self):
        for elements in (1, 2, 3, 5):
            raw = elements * 0x28 + elements * 4
            with self.subTest(elements=elements):
                self.assertEqual(self._size(elements), (raw + 0xF) & ~0xF)

    def test_an_already_aligned_total_is_left_alone(self):
        # Four elements: 4*0x28 + 4*4 = 176, already a multiple of 16.
        self.assertEqual(self._size(4), 176)
        self.assertEqual(176 % 16, 0)


class RetailManifestTests(unittest.TestCase):
    """The Mission 01 retail manifests, and what they are allowed to claim."""

    @staticmethod
    def _record(index, class_byte, faction_index, objects):
        return {
            "index": index,
            "class_byte": class_byte,
            "faction_index": faction_index,
            "object_category": CLASS_TO_OBJECT_CATEGORY[class_byte],
            "has_behaviour_set": True,
            "objects": objects,
        }

    def test_the_category_map_is_the_switch_at_0x820a72e0(self):
        # 0 -> 1, 1 -> 4, 2 -> 4, 3 -> 4, 4 -> 3. Nothing else is implemented,
        # and every Mission 01 record falls inside that domain.
        self.assertEqual(CLASS_TO_OBJECT_CATEGORY, {0: 1, 1: 4, 2: 4, 3: 4, 4: 3})

    def test_a_wave_row_carries_the_record_fields_and_declared_placeholders(self):
        rows = waves_rows([self._record(0, 2, 1, [(1.5, -2.0, 3.25)])])
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual(len(row), 12)
        self.assertEqual(row[0], "1")
        self.assertEqual(row[1], "1")                       # one load-time pass
        self.assertEqual(row[2], str(ENTITY_BASE))          # element index 0
        self.assertEqual(row[3], "2")                       # faction byte + 1
        self.assertEqual(row[4], "4")                       # class byte 2 -> 4
        self.assertEqual(row[5], row[3])                    # faction == owner
        self.assertEqual(row[6:9], ["1.500000", "-2.000000", "3.250000"])
        self.assertEqual(row[9:], ["1.000000", "1.000000", "1.000000"])

    def test_a_record_without_an_obj_child_gets_the_origin_not_an_invention(self):
        row = waves_rows([self._record(3, 0, 0, [])])[0]
        self.assertEqual(row[6:9], ["0.000000", "0.000000", "0.000000"])
        self.assertEqual(row[2], str(ENTITY_BASE + 3))

    def test_unit_ids_never_collide_with_owner_ids(self):
        # The native registry rejects a unit whose id equals its owner id, so
        # the entity base has to keep the two ranges apart.
        rows = waves_rows([self._record(index, 1, index % 3, [])
                           for index in range(8)])
        for row in rows:
            self.assertNotEqual(row[2], row[3])

    def test_objectives_are_emitted_with_four_columns_so_the_condition_is_manual(self):
        rows = objectives_rows([{"index": 0, "steps": [0, 1]},
                                {"index": 1, "steps": [0]}])
        self.assertEqual([len(row) for row in rows], [4, 4])
        self.assertEqual(rows[0], ["1", "1", "mission01-submission-0", "1"])
        self.assertEqual(rows[1][2], "mission01-submission-1")


class ReaderDigestTests(unittest.TestCase):
    """The digest the C++ readers are replayed against."""

    def test_runs_are_hashed_in_address_order_not_file_order(self):
        first = {"address": "0xb4000000", "size": 2, "after_hex": "0102"}
        second = {"address": "0xb5000000", "size": 1, "after_hex": "ff"}
        forward, runs, written = digest_canonical([first, second])
        backward, _, _ = digest_canonical([second, first])
        self.assertEqual(forward, backward)
        self.assertEqual(runs, 2)
        self.assertEqual(written, 3)
        self.assertEqual(forward, "b4000000:2:0102\nb5000000:1:ff\n")

    def test_a_declared_size_that_contradicts_the_bytes_is_an_error(self):
        with self.assertRaises(ValueError):
            digest_canonical([{"address": "0xb4000000", "size": 4, "after_hex": "0102"}])

    def test_the_hash_is_fnv1a_64_and_moves_with_one_byte(self):
        # The FNV-1a 64 offset basis, hashed over the empty string.
        self.assertEqual(digest_fnv64(""), 0xCBF29CE484222325)
        self.assertNotEqual(digest_fnv64("b4000000:1:00\n"),
                            digest_fnv64("b4000000:1:01\n"))

    def test_the_committed_table_covers_the_whole_family(self):
        path = TOOLS.parent / "analysis" / "microexec" / "reader-digests.tsv"
        rows = [line.split("\t") for line in path.read_text().splitlines()
                if line and not line.startswith("#")]
        self.assertEqual(len(rows), 138)
        self.assertEqual(len({row[0] for row in rows}), 10)
        for row in rows:
            self.assertEqual(len(row), 5)
            self.assertEqual(len(row[4]), 16)


class ScenarioRoundTripTests(unittest.TestCase):
    """Parse then re-emit, on containers built here rather than read from retail."""

    @staticmethod
    def _payload(child_count: int = 2, data: bytes = b"\x01\x02\x03\x04") -> bytes:
        """A root with `child_count` children, each carrying a data block."""
        # Layout: root node, root table, then one node + data block per child.
        blob = bytearray()

        def u32(value: int) -> None:
            blob.extend(struct.pack(">I", value))

        u32(0)                       # the root has no data block
        u32(8)                       # its table follows immediately
        table = len(blob)
        u32(child_count)
        slots = []
        for _ in range(child_count):
            slots.append(len(blob))
            u32(0)
        for index in range(child_count):
            node = len(blob)
            struct.pack_into(">I", blob, slots[index], node - table)
            u32(16)                  # data block sits 16 bytes past the node
            u32(0)                   # no table of its own
            blob.extend(b"\x00" * 8)  # padding the walk will not claim
            blob.extend(data)
        return bytes(blob)

    def _roundtrip(self, raw: bytes):
        payload = NativePayload(raw)
        walk = ScenarioWalk(payload)
        rebuilt, written = scenario_reemit(payload, walk)
        return walk, rebuilt, written

    def test_a_container_is_reproduced_byte_for_byte(self):
        raw = self._payload()
        walk, rebuilt, written = self._roundtrip(raw)
        self.assertEqual(rebuilt, raw)
        self.assertEqual(len(walk.nodes), 3)      # the root and its two children
        self.assertEqual(len(walk.tables), 1)
        self.assertEqual(len(walk.data), 2)

    def test_unclaimed_bytes_are_the_padding_and_are_zero(self):
        raw = self._payload()
        _, _, written = self._roundtrip(raw)
        unclaimed = [index for index, flag in enumerate(written) if not flag]
        self.assertTrue(unclaimed)                # the eight-byte gaps
        self.assertTrue(all(raw[index] == 0 for index in unclaimed))

    def test_an_unclaimed_non_zero_byte_breaks_the_contract(self):
        # The same container with one byte written into the padding: the walk
        # never sees it, so the rebuilt file must differ.
        raw = bytearray(self._payload())
        _, _, written = self._roundtrip(bytes(raw))
        gap = next(index for index, flag in enumerate(written) if not flag)
        raw[gap] = 0xAB
        _, rebuilt, _ = self._roundtrip(bytes(raw))
        self.assertNotEqual(rebuilt, bytes(raw))

    def test_the_structure_is_recomputed_and_not_copied(self):
        # Tampering with the model, not the bytes, must move the output. If the
        # emitter copied structural words the mutation would be invisible.
        raw = self._payload()
        payload = NativePayload(raw)
        walk = ScenarioWalk(payload)
        table = next(iter(walk.tables))
        walk.tables[table] = [offset + 4 for offset in walk.tables[table]]
        rebuilt, _ = scenario_reemit(payload, walk)
        self.assertNotEqual(rebuilt, raw)

    def test_a_negative_count_is_kept_and_not_followed(self):
        raw = bytearray(self._payload())
        payload = NativePayload(bytes(raw))
        walk = ScenarioWalk(payload)
        table = next(iter(walk.tables))
        struct.pack_into(">i", raw, table, -1)
        walk2 = ScenarioWalk(NativePayload(bytes(raw)))
        self.assertEqual(walk2.tables[table], [])
        self.assertEqual(len(walk2.nodes), 1)     # the children are not reached


class ObjectLayoutMapping(unittest.TestCase):
    """`map_object_layout.py` -- the constructor-to-subobject tracer.

    The failures it is built against are all real. Cycle 1370's throwaway version
    guarded the type descriptor with `< 0x82400000` and so reported every AC6
    class as unnamed, because the descriptors live at 0x8268F000. And the same
    integer appears in this binary both as a displacement (`this + 2224`) and as
    the low half of a string address (0x820008B0); telling them apart is the
    whole reason constants and this-relative pointers are separate lattices.
    """

    def setUp(self):
        self.corpus = tempfile.TemporaryDirectory()
        self.addCleanup(self.corpus.cleanup)

    def _corpus(self, functions):
        """functions: {address: [(mnemonic, operands), ...]} -> a corpus dir."""
        lines = []
        for address, body in sorted(functions.items()):
            lines.append("PPC_FUNC_IMPL(__imp__sub_%08X) {" % address)
            for mnemonic, operands in body:
                lines.append("\t// %s %s" % (mnemonic, operands))
            lines.append("}")
            lines.append("")
        path = Path(self.corpus.name) / "ppc_recomp.0.cpp"
        path.write_text("\n".join(lines) + "\n")
        return self.corpus.name

    @staticmethod
    def _image_with_class(vtable, name):
        """A flat image carrying one vtable with a working COL chain."""
        data = bytearray(0x00090000)          # 0x82000000 .. 0x82090000

        def put(address, word):
            struct.pack_into(">I", data, address - 0x82000000, word)

        locator = 0x82060000
        descriptor = 0x82070000
        put(vtable - 4, locator)
        put(locator + 0x0C, descriptor)
        encoded = name.encode("ascii") + b"\0"
        start = descriptor + 8 - 0x82000000
        data[start:start + len(encoded)] = encoded
        return bytes(data)

    def test_a_named_vtable_install_is_reported_at_its_this_offset(self):
        vtable = 0x82054D94
        corpus = self._corpus({0x82001000: [
            ("mflr", "r12"),
            ("mr", "r31,r3"),
            ("lis", "r11,%d" % ((vtable >> 16) - 0x10000)),
            ("addi", "r11,r11,%d" % (vtable & 0xFFFF)),
            ("stw", "r11,4928(r31)"),
        ]})
        image = self._image_with_class(vtable, ".?AVCGaLocator@galib@@")
        functions = map_object_layout.load_corpus(corpus)
        installs, _, _ = map_object_layout.map_layout(functions, image, 0x82001000)
        self.assertEqual(installs, [(0x82001010, 4928, vtable,
                                     ".?AVCGaLocator@galib@@")])

    def test_a_descriptor_past_the_rdata_bound_is_still_read(self):
        # The exact defect of the throwaway version: AC6 keeps type descriptors
        # far above every section this campaign usually reads. A guard tighter
        # than the image renames every class to None and looks like a binary
        # without RTTI.
        data = bytearray(0x00700000)          # image runs to 0x82700000
        vtable, locator, descriptor = 0x82054D94, 0x82060000, 0x8268F748

        def put(address, word):
            struct.pack_into(">I", data, address - 0x82000000, word)

        put(vtable - 4, locator)
        put(locator + 0x0C, descriptor)
        encoded = b".?AVCAce6Thread@ACE6@@\0"
        start = descriptor + 8 - 0x82000000
        data[start:start + len(encoded)] = encoded
        self.assertEqual(map_object_layout.rtti_name(bytes(data), vtable),
                         ".?AVCAce6Thread@ACE6@@")

    def test_a_string_address_is_not_read_as_a_displacement(self):
        # `addi r10,r11,2224` off a CONSTANT base is 0x820008B0, a string.
        # `addi r3,r31,2224` off `this` is the subobject at +2224. Same integer,
        # and the tool must report exactly one of them.
        corpus = self._corpus({0x82001000: [
            ("mr", "r31,r3"),
            ("lis", "r11,-32256"),
            ("addi", "r10,r11,2224"),          # 0x820008B0 -- a string
            ("addi", "r3,r31,2224"),           # this+2224  -- a subobject
            ("bl", "0x82302b28"),
        ]})
        image = self._image_with_class(0x82054D94, ".?AVX@@")
        functions = map_object_layout.load_corpus(corpus)
        installs, calls, _ = map_object_layout.map_layout(functions, image, 0x82001000)
        self.assertEqual(installs, [])
        self.assertEqual(calls, [(0x82001010, 2224, 0x82302B28)])

    def test_assigning_a_constant_clears_a_this_relative_binding(self):
        # A register holds one kind of value at a time. If `mr` left the old
        # relative binding in place, the later store would be attributed to an
        # offset the code never computes.
        corpus = self._corpus({0x82001000: [
            ("mr", "r31,r3"),
            ("addi", "r9,r31,64"),             # r9 is this+64
            ("lis", "r9,-32256"),              # ... and now it is a constant
            ("addi", "r9,r9,4"),
            ("stw", "r9,8(r31)"),
        ]})
        image = self._image_with_class(0x82054D94, ".?AVX@@")
        functions = map_object_layout.load_corpus(corpus)
        installs, _, _ = map_object_layout.map_layout(functions, image, 0x82001000)
        self.assertEqual([(offset, value) for _, offset, value, _ in installs],
                         [(8, 0x82000004)])

    def test_unfollowed_branches_are_counted_rather_than_hidden(self):
        corpus = self._corpus({0x82001000: [
            ("mr", "r31,r3"),
            ("beq", "0x82001020"),
            ("b", "0x82001030"),
        ]})
        image = self._image_with_class(0x82054D94, ".?AVX@@")
        functions = map_object_layout.load_corpus(corpus)
        _, _, branches = map_object_layout.map_layout(functions, image, 0x82001000)
        self.assertEqual(branches, 2)


class CalibrationNormalisationTests(unittest.TestCase):
    """The exclusion that let the calibration report 0 of 138 for 87 commits.

    `region_dumps` is emitted by the general harness always, as `[]` when no
    spec asked for a dump, and the specialised harness that produced the 138
    committed snapshots had no such key. Dropping it unconditionally would make
    the calibration blind to a spec that gained a real dump, so it is dropped
    only when empty -- and that distinction is what these two tests pin.
    """

    def _normalise(self):
        import importlib.util
        path = (Path(__file__).resolve().parents[1]
                / "audit_microexec_harness_calibration.py")
        spec = importlib.util.spec_from_file_location("calibration", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module.normalise

    def test_an_empty_region_dumps_is_dropped(self):
        normalise = self._normalise()
        reference = {"exit": {"kind": "return"}, "registers": {}}
        candidate = {"exit": {"kind": "return"}, "registers": {}, "region_dumps": []}
        self.assertEqual(normalise(reference), normalise(candidate))

    def test_a_non_empty_region_dumps_is_kept_and_still_differs(self):
        normalise = self._normalise()
        reference = {"exit": {"kind": "return"}, "registers": {}}
        candidate = {"exit": {"kind": "return"}, "registers": {},
                     "region_dumps": [{"name": "out", "after_hex": "00"}]}
        self.assertNotEqual(normalise(reference), normalise(candidate))
