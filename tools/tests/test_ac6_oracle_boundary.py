from __future__ import annotations

import io
import hashlib
import json
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import unittest
from contextlib import redirect_stdout
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

import audit_ac6_product_boundary as boundary
import audit_native_package
from audit_ac6_oracle_manifest import ManifestError, tree_sha256, validate_document
from apply_ac6_oracle_boundary_corrections import CorrectionError, correct_configuration
from build_ac6_oracle_runtime_config import (
    RuntimeConfigError,
    apply_hook_policy,
    load_hook_policy,
)
from build_ac6_oracle_capture_config import (
    CaptureConfigError,
    apply_overrides,
    load_policy as load_capture_policy,
)
from generate_ac6_oracle_config import (
    BoundaryError,
    append_switch_tables,
    derive_configuration,
    load_boundaries,
    load_interceptions,
    load_switch_tables,
)
from normalize_ac6_recomp_trace import TraceError, load_events

MANIFEST = ROOT / "analysis/oracle/ac6-recomp-dcd41b/manifest.json"
RAW_TRACE = ROOT / "analysis/oracle/ac6-recomp-dcd41b/fixtures/mission01-frame.raw.jsonl"
ROUTE_CAPTURE = (
    ROOT / "analysis/oracle/ac6-recomp-dcd41b/captures/mission01-hud-route"
)
PATCH_STACK = ROOT / "analysis/oracle/ac6-recomp-dcd41b/patches/stack.json"


class OracleManifestTests(unittest.TestCase):
    def test_versioned_manifest_and_probe_hash_validate(self) -> None:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(validate_document(document, ROOT), 1)

    def test_wrong_oracle_commit_is_rejected(self) -> None:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        document["oracle"]["commit"] = "0" * 40
        with self.assertRaises(ManifestError):
            validate_document(document, ROOT)

    def test_wrong_overlay_tree_is_rejected(self) -> None:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        document["build_overlay"]["tree_sha1"] = "0" * 40
        with self.assertRaises(ManifestError):
            validate_document(document, ROOT)

    def test_wrong_boundary_transformer_is_rejected(self) -> None:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        document["boundary_correction"]["transformer_sha256"] = "0" * 64
        with self.assertRaises(ManifestError):
            validate_document(document, ROOT)

    def test_generated_tree_digest_covers_names_and_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "a").write_bytes(b"bc")
            before = tree_sha256(root)
            (root / "a").rename(root / "b")
            after = tree_sha256(root)
            self.assertEqual(before[:2], after[:2])
            self.assertNotEqual(before[2], after[2])

    def test_ordered_host_patch_stack_is_sealed(self) -> None:
        stack = json.loads(PATCH_STACK.read_text(encoding="utf-8"))
        self.assertEqual(stack["schema"], "ac6.oracle-host-patch-stack.v1")
        self.assertEqual(
            [record["order"] for record in stack["patches"]],
            list(range(1, len(stack["patches"]) + 1)),
        )
        for record in stack["patches"]:
            path = ROOT / record["path"]
            self.assertTrue(path.is_file(), record["path"])
            self.assertEqual(
                hashlib.sha256(path.read_bytes()).hexdigest(),
                record["sha256"],
                record["path"],
            )


class OracleBoundaryCorrectionTests(unittest.TestCase):
    def test_exact_entries_are_removed_without_other_rewrite(self) -> None:
        source = (
            b'[functions]\n'
            b'0x82000000 = { name = "rex_sub_82000000" }\n'
            b'keep = "literal"\n'
            b'0x82000004 = { name = "rex_sub_82000004" }\n'
        )
        corrected = correct_configuration(source, ["0x82000000", "0x82000004"])
        self.assertEqual(corrected, b'[functions]\nkeep = "literal"\n')

    def test_missing_entry_is_rejected(self) -> None:
        with self.assertRaises(CorrectionError):
            correct_configuration(b"[functions]\n", ["0x82000000"])

    def test_duplicate_address_is_rejected(self) -> None:
        source = b'0x82000000 = { name = "rex_sub_82000000" }\n'
        with self.assertRaises(CorrectionError):
            correct_configuration(source, ["0x82000000", "0x82000000"])


class OracleBoundaryExportTests(unittest.TestCase):
    def boundary_document(self) -> dict:
        return {
            "schema": "ac6.ghidra-function-boundaries.v1",
            "project": "ace-combat-6",
            "program": "default.xex",
            "sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
            "language": "PowerPC:BE:64:A2ALT-32addr",
            "function_count": 1,
            "functions": [{
                "entry": "0x82000000",
                "ranges": [["0x82000000", "0x8200000C"]],
            }],
        }

    def test_internal_config_start_is_removed(self) -> None:
        functions = load_boundaries(self.boundary_document())
        source = (
            b'0x82000000 = { name = "rex_sub_82000000" }\n'
            b'0x82000008 = { name = "rex_sub_82000008" }\n'
            b'0x82000010 = { name = "rex_sub_82000010" }\n'
        )
        output, removed, retained, preserved = derive_configuration(source, functions)
        self.assertEqual([item["candidate"] for item in removed], ["0x82000008"])
        self.assertEqual(retained, 2)
        self.assertEqual(preserved, [])
        self.assertNotIn(b"0x82000008", output)

    def test_declared_internal_interception_is_preserved(self) -> None:
        functions = load_boundaries(self.boundary_document())
        document = {
            "schema": "ac6.oracle-host-interceptions.v1",
            "project": "ace-combat-6",
            "program": "default.xex",
            "sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
            "interceptions": [{
                "candidate": "0x82000008",
                "owner": "0x82000000",
                "symbol": "rex_sub_82000008",
            }],
        }
        interceptions = load_interceptions(document, functions)
        source = (
            b'0x82000004 = { name = "rex_sub_82000004" }\n'
            b'0x82000008 = { name = "rex_sub_82000008" }\n'
        )
        output, removed, retained, preserved = derive_configuration(
            source, functions, interceptions
        )
        self.assertEqual(output, b'0x82000008 = { name = "rex_sub_82000008" }\n')
        self.assertEqual([item["candidate"] for item in removed], ["0x82000004"])
        self.assertEqual(retained, 1)
        self.assertEqual(preserved, document["interceptions"])

    def test_overlapping_canonical_bodies_are_rejected(self) -> None:
        document = self.boundary_document()
        document["function_count"] = 2
        document["functions"].append({
            "entry": "0x82000008",
            "ranges": [["0x82000008", "0x82000010"]],
        })
        with self.assertRaises(BoundaryError):
            load_boundaries(document)

    def test_qualified_switch_table_is_appended_without_new_boundary(self) -> None:
        functions = load_boundaries(self.boundary_document())
        document = {
            "schema": "ac6.oracle-switch-tables.v1",
            "project": "ace-combat-6",
            "program": "default.xex",
            "sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
            "switch_tables": [{
                "address": "0x82000004",
                "owner": "0x82000000",
                "register": 10,
                "labels": ["0x82000000", "0x82000008"],
                "evidence": "fixture",
            }],
        }
        tables = load_switch_tables(document, functions)
        output = append_switch_tables(b"[functions]\n", tables)
        self.assertIn(b"address = 0x82000004", output)
        self.assertNotIn(b"rex_sub_82000004", output)

    def test_switch_table_outside_canonical_owner_is_rejected(self) -> None:
        functions = load_boundaries(self.boundary_document())
        document = {
            "schema": "ac6.oracle-switch-tables.v1",
            "project": "ace-combat-6",
            "program": "default.xex",
            "sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
            "switch_tables": [{
                "address": "0x82000010",
                "owner": "0x82000000",
                "register": 10,
                "labels": ["0x82000000", "0x82000008"],
                "evidence": "fixture",
            }],
        }
        with self.assertRaises(BoundaryError):
            load_switch_tables(document, functions)

    def test_runtime_hook_policy_disables_only_declared_hook(self) -> None:
        source = (
            b'[[midasm_hook]]\naddress = 0x82000000\nname = "keep"\n\n'
            b'[[midasm_hook]]\naddress = 0x82000004\nname = "drop"\n\n'
        )
        digest = hashlib.sha256(source).hexdigest()
        document = {
            "schema": "ac6.oracle-midasm-hook-policy.v1",
            "input_configuration_sha256": digest,
            "hooks": [
                {"address": "0x82000000", "name": "keep",
                 "disposition": "retain", "reason": "fixture"},
                {"address": "0x82000004", "name": "drop",
                 "disposition": "disable", "reason": "fixture"},
            ],
        }
        output = apply_hook_policy(source, load_hook_policy(document, digest))
        self.assertIn(b'address = 0x82000000', output)
        self.assertIn(b'# oracle-disabled: address = 0x82000004', output)

    def test_runtime_hook_policy_must_cover_exact_inventory(self) -> None:
        source = b'[[midasm_hook]]\naddress = 0x82000000\nname = "actual"\n'
        digest = hashlib.sha256(source).hexdigest()
        document = {
            "schema": "ac6.oracle-midasm-hook-policy.v1",
            "input_configuration_sha256": digest,
            "hooks": [{"address": "0x82000000", "name": "other",
                       "disposition": "retain", "reason": "fixture"}],
        }
        with self.assertRaises(RuntimeConfigError):
            apply_hook_policy(source, load_hook_policy(document, digest))

    def test_capture_override_inserts_qualified_indirect_wrapper(self) -> None:
        source = (
            b'[functions]\n'
            b'0x82000000 = { name = "rex_sub_82000000" }\n'
            b'0x82000008 = { name = "rex_sub_82000008" }\n\n'
            b'[[midasm_hook]]\naddress = 0x82000004\nname = "fixture"\n'
        )
        output = apply_overrides(
            source, [(0x82000004, "rex_sub_82000004", "fixture")]
        )
        self.assertIn(
            b'0x82000000 = { name = "rex_sub_82000000" }\n'
            b'0x82000004 = { name = "rex_sub_82000004" }\n'
            b'0x82000008 = { name = "rex_sub_82000008" }',
            output,
        )
        tomllib.loads(output.decode("utf-8"))

    def test_capture_overrides_keep_multiple_insertions_sorted(self) -> None:
        source = (
            b'[functions]\n'
            b'0x82000000 = { name = "rex_sub_82000000" }\n'
            b'0x8200000C = { name = "rex_sub_8200000C" }\n'
        )
        output = apply_overrides(source, [
            (0x82000004, "rex_sub_82000004", "first fixture"),
            (0x82000008, "rex_sub_82000008", "second fixture"),
        ])
        self.assertEqual(
            output,
            b'[functions]\n'
            b'0x82000000 = { name = "rex_sub_82000000" }\n'
            b'0x82000004 = { name = "rex_sub_82000004" }\n'
            b'0x82000008 = { name = "rex_sub_82000008" }\n'
            b'0x8200000C = { name = "rex_sub_8200000C" }\n',
        )

    def test_capture_override_requires_canonical_entry(self) -> None:
        document = {
            "schema": "ac6.oracle-capture-function-overrides.v1",
            "input_configuration_sha256": "1" * 64,
            "boundary_export_sha256": "2" * 64,
            "overrides": [{
                "address": "0x82000004",
                "name": "rex_sub_82000004",
                "evidence": "fixture",
            }],
        }
        with self.assertRaises(CaptureConfigError):
            load_capture_policy(
                json.dumps(document).encode(), "1" * 64, "2" * 64,
                {0x82000000},
            )


class OracleTraceTests(unittest.TestCase):
    def test_fixture_normalizes_deterministically(self) -> None:
        events = load_events(RAW_TRACE, 20_000)
        self.assertEqual(len(events), 2)
        self.assertEqual(events[0]["guest_address"], "0x82267370")
        self.assertEqual(events[1]["input"]["buttons"], ["a"])
        self.assertEqual(list(events[1]["output_hashes"]), ["color", "simulation"])

    def test_tick_regression_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "raw.jsonl"
            event = json.loads(RAW_TRACE.read_text(encoding="utf-8").splitlines()[0])
            first = {**event, "tick": 2}
            second = {**event, "tick": 1}
            path.write_text(json.dumps(first) + "\n" + json.dumps(second) + "\n",
                            encoding="utf-8")
            with self.assertRaises(TraceError):
                load_events(path, 2)


class OracleRouteQualificationTests(unittest.TestCase):
    def test_route_artifacts_and_manifests_are_sealed(self) -> None:
        qualification = json.loads(
            (ROUTE_CAPTURE / "qualification.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            qualification["schema"], "ac6.oracle-route-qualification.v1"
        )
        for run in qualification["runs"]:
            manifest_path = ROUTE_CAPTURE / run["manifest"]
            raw_path = ROUTE_CAPTURE / run["raw_trace"]
            self.assertEqual(
                hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                run["manifest_sha256"],
            )
            self.assertEqual(
                hashlib.sha256(raw_path.read_bytes()).hexdigest(),
                run["raw_trace_sha256"],
            )
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["binary"]["sha256"],
                             qualification["binary_sha256"])
            self.assertEqual(manifest["route"]["sha256"],
                             qualification["route"]["sha256"])
            self.assertEqual(manifest["route"]["executed_steps"], 96)
            self.assertEqual(manifest["fatal_matches"], [])
            self.assertEqual(manifest["shared_memory_before"],
                             manifest["shared_memory_after"])

    def test_guest_hashes_inputs_and_hud_are_reproduced(self) -> None:
        qualification = json.loads(
            (ROUTE_CAPTURE / "qualification.json").read_text(encoding="utf-8")
        )
        events = []
        for run in qualification["runs"]:
            events.append([
                json.loads(line)
                for line in (ROUTE_CAPTURE / run["raw_trace"])
                .read_text(encoding="utf-8").splitlines()
            ])
        self.assertEqual(
            [event["input"] for event in events[0]],
            [event["input"] for event in events[1]],
        )
        for records in events:
            self.assertEqual(
                [event["output_hashes"]["scheduler_registers"]
                 for event in records],
                qualification["reproduced"]["scheduler_register_hashes"],
            )
        for run_id in ("ay", "az"):
            self.assertEqual(
                hashlib.sha256(
                    (ROUTE_CAPTURE / f"{run_id}-flight-hud.png").read_bytes()
                ).hexdigest(),
                qualification["reproduced"]["flight_hud_sha256"],
            )
            self.assertEqual(
                hashlib.sha256(
                    (ROUTE_CAPTURE / f"{run_id}-flight-throttle.png").read_bytes()
                ).hexdigest(),
                qualification["reproduced"]["flight_throttle_sha256"],
            )


class ProductBoundaryTests(unittest.TestCase):
    def test_clean_source_and_staging_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            staging = root / "staging"
            source.mkdir()
            staging.mkdir()
            (source / "runtime.cpp").write_text("int native_runtime() { return 0; }\n",
                                                encoding="utf-8")
            (staging / "ac6-native").write_bytes(b"native")
            self.assertEqual(boundary.audit_source(source), 1)
            self.assertEqual(boundary.audit_staging(staging), 1)

    def test_oracle_source_marker_and_generated_staging_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "runtime.cpp").write_text("void rex_bridge();\n", encoding="utf-8")
            with self.assertRaises(boundary.BoundaryError):
                boundary.audit_source(root)
            generated = root / "stage/generated"
            generated.mkdir(parents=True)
            with self.assertRaises(boundary.BoundaryError):
                boundary.audit_staging(root / "stage")

    def test_package_rejects_oracle_named_entry(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            archive_path = Path(temporary) / "bad.tar.gz"
            payload = b"not retail"
            with tarfile.open(archive_path, "w:gz") as archive:
                info = tarfile.TarInfo("ac6/generated/unit.cpp")
                info.size = len(payload)
                archive.addfile(info, io.BytesIO(payload))
            saved = sys.argv
            sys.argv = ["audit_native_package.py", str(archive_path)]
            try:
                with self.assertRaises(SystemExit):
                    with redirect_stdout(io.StringIO()):
                        audit_native_package.main()
            finally:
                sys.argv = saved

    def test_current_product_source_passes(self) -> None:
        checked = boundary.audit_source(ROOT / "reconstruction/ace-combat-6")
        self.assertGreater(checked, 100)


if __name__ == "__main__":
    unittest.main()
