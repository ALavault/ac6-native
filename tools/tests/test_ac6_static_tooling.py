from __future__ import annotations

import hashlib
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from ac6_mode1_codec import descramble
from audit_ac6_mission01_native_gate import GateError, audit_contract, template
from build_ac6_asset_closure import build_closure
from compare_ac6_asset_closures import compare as compare_closures
from compare_ac6_function_snapshots import compare_three
from extract_ac6_pac import extract_selected


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
            document = template()
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
            document = template()
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


if __name__ == "__main__":
    unittest.main()
