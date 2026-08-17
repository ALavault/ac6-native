from __future__ import annotations

import io
import hashlib
import json
import re
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

import audit_ac6_product_boundary as boundary
import audit_native_package
import apply_ac6_oracle_patch_stack as patch_stack
from audit_ac6_oracle_manifest import ManifestError, tree_sha256, validate_document
from apply_ac6_oracle_boundary_corrections import CorrectionError, correct_configuration
from apply_ac6_oracle_patch_stack import (
    PatchRecord,
    StackError,
    apply_stack,
    load_stack,
    preflight,
    validate_target,
)
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
MOVIE_PATCH_STACK = (
    ROOT / "analysis/oracle/ac6-recomp-dcd41b/patches/stack-xam-input-movie-v1.json"
)


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
            patch_record = PatchRecord(
                record["order"], path, record["path"],
                tuple(record.get("apply_args", [])),
            )
            patch_stack.record_paths(ROOT, patch_record)
        replay_patch = ROOT / stack["patches"][11]["path"]
        patch_text = replay_patch.read_text(encoding="utf-8")
        file_diffs = [
            "diff --git " + section
            for section in patch_text.split("diff --git ")[1:]
        ]
        self.assertTrue(file_diffs)
        for file_diff in file_diffs:
            hunk_headers = re.findall(
                r"^@@ -\d+(?:,(\d+))? \+\d+(?:,(\d+))? @@",
                file_diff,
                flags=re.MULTILINE,
            )
            self.assertTrue(hunk_headers)
            if "\n--- /dev/null\n" not in file_diff:
                self.assertTrue(
                    all(old_count != "0" for old_count, _ in hunk_headers)
                )

    def test_patch_stack_loader_qualifies_all_records(self) -> None:
        base, records = load_stack(PATCH_STACK, ROOT)
        self.assertEqual(base, "dcd41b7457fcac8242f8ef40de83d1719390d5af")
        self.assertEqual(len(records), 13)
        self.assertEqual(records[11].display_path,
                         "analysis/oracle/ac6-recomp-dcd41b/patches/"
                         "deterministic-trace-input-replay.patch")

    def test_xam_input_movie_stack_extends_the_sealed_stack(self) -> None:
        # reproducibility-v1.json seals the 13-patch overlay of stack.json;
        # the XAM input movie lane carries the same 13 records plus two more
        # in a separate stack so the seal and the movie build stay distinct.
        sealed_base, sealed = load_stack(PATCH_STACK, ROOT)
        movie_base, movie = load_stack(MOVIE_PATCH_STACK, ROOT)
        self.assertEqual(movie_base, sealed_base)
        self.assertEqual(len(movie), 15)
        self.assertEqual(
            [(r.display_path, r.apply_args) for r in movie[:13]],
            [(r.display_path, r.apply_args) for r in sealed],
        )
        self.assertEqual(
            [r.display_path.rsplit("/", 1)[1] for r in movie[13:]],
            ["poll-exact-xam-controller-replay.patch",
             "xam-input-movie-v1.patch"],
        )
        sealed_doc = json.loads(PATCH_STACK.read_text(encoding="utf-8"))
        movie_doc = json.loads(MOVIE_PATCH_STACK.read_text(encoding="utf-8"))
        self.assertEqual(movie_doc["configuration"], sealed_doc["configuration"])
        self.assertEqual(movie_doc["qualification"]["qualified_patch_count"], 13)


class OraclePatchStackTransactionTests(unittest.TestCase):
    def git(self, root: Path, *arguments: str) -> str:
        result = subprocess.run(
            ["git", "-C", str(root), *arguments], check=True,
            capture_output=True, text=True,
        )
        return result.stdout.rstrip("\n")

    def repository(self, temporary: str) -> tuple[Path, str]:
        root = Path(temporary) / "runtime"
        (root / "assets").mkdir(parents=True)
        (root / "ac6recomp_config.toml").write_text("base\n", encoding="utf-8")
        (root / "assets/default.xex").write_bytes(b"PAL fixture")
        (root / "a.txt").write_text("one\n", encoding="utf-8")
        (root / "b.txt").write_text("one\n", encoding="utf-8")
        self.git(root, "init", "-q")
        self.git(root, "config", "user.name", "AC6 test")
        self.git(root, "config", "user.email", "ac6-test@example.invalid")
        self.git(root, "add", ":/")
        self.git(root, "commit", "-qm", "fixture")
        base = self.git(root, "rev-parse", "HEAD")
        self.git(root, "switch", "--detach", "-q", base)
        (root / "ac6recomp_config.toml").write_text("corrected\n", encoding="utf-8")
        return root, base

    def records(self, temporary: str) -> list[PatchRecord]:
        patch_root = Path(temporary) / "patches"
        patch_root.mkdir()
        records = []
        for order, name in enumerate(("a.txt", "b.txt"), start=1):
            path = patch_root / f"{order}.patch"
            path.write_text(
                f"diff --git a/{name} b/{name}\n"
                f"--- a/{name}\n+++ b/{name}\n@@ -1 +1 @@\n-one\n+two\n",
                encoding="utf-8",
            )
            records.append(PatchRecord(order, path, path.name, ()))
        return records

    def manifest(self, temporary: str, base: str, xex: Path) -> Path:
        artifact_root = Path(temporary) / "artifacts"
        manifest = artifact_root / "analysis/oracle/ac6-recomp-dcd41b/manifest.json"
        manifest.parent.mkdir(parents=True)
        manifest.write_text(json.dumps({
            "oracle": {"commit": base},
            "target": {
                "module": "default.xex",
                "platform": "Xbox 360 PAL",
                "size": xex.stat().st_size,
                "sha256": hashlib.sha256(xex.read_bytes()).hexdigest(),
            },
            "configuration": {"path": "ac6recomp_config.toml"},
            "boundary_correction": {
                "patched_configuration_sha256": hashlib.sha256(
                    b"corrected\n"
                ).hexdigest(),
            },
        }), encoding="utf-8")
        return artifact_root

    def test_target_cross_checks_manifest_commit_and_pal_xex(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, base = self.repository(temporary)
            artifact_root = self.manifest(
                temporary, base, root / "assets/default.xex"
            )
            validate_target(root, base, artifact_root)
            document_path = artifact_root / "analysis/oracle/ac6-recomp-dcd41b/manifest.json"
            document = json.loads(document_path.read_text(encoding="utf-8"))
            document["oracle"]["commit"] = "0" * 40
            document_path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(StackError, "stack/manifest oracle commit"):
                validate_target(root, base, artifact_root)

    def test_traditional_unified_patch_paths_are_snapshotted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, _ = self.repository(temporary)
            patch_path = Path(temporary) / "traditional.patch"
            patch_path.write_text(
                "--- a/a.txt\n+++ b/a.txt\n@@ -1 +1 @@\n-one\n+two\n",
                encoding="utf-8",
            )
            record = PatchRecord(1, patch_path, patch_path.name, ())

            self.assertEqual(patch_stack.record_paths(root, record), {Path("a.txt")})

    def test_generated_output_patch_paths_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, _ = self.repository(temporary)
            for relative in ("generated/recomp.cpp", "src/ppc_recomp.cpp"):
                with self.subTest(relative=relative):
                    patch_path = Path(temporary) / (relative.replace("/", "-") + ".patch")
                    patch_path.write_text(
                        f"diff --git a/{relative} b/{relative}\n"
                        f"--- /dev/null\n+++ b/{relative}\n"
                        "@@ -0,0 +1 @@\n+forbidden\n",
                        encoding="utf-8",
                    )
                    record = PatchRecord(1, patch_path, patch_path.name, ())
                    with self.assertRaisesRegex(
                        StackError, "generated output path forbidden"
                    ):
                        patch_stack.record_paths(root, record)

    def test_overlay_qualification_is_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, base = self.repository(temporary)
            records = self.records(temporary)
            result = patch_stack.preflight_details(root, base, records)
            stack_path = Path(temporary) / "stack.json"
            qualification = {
                "qualified_patch_count": 0,
                "clean_application_pass": True,
                "runtime_route_status": "open",
                "changed_file_count": result.changed_file_count,
                "changed_tree_algorithm":
                    "sha256(sorted(path + NUL + bytes + NUL))",
                "changed_tree_sha256": result.changed_tree_sha256,
                "capture_profile_byte_match": True,
            }
            stack_path.write_text(
                json.dumps({"qualification": qualification}), encoding="utf-8"
            )
            patch_stack.validate_qualification(stack_path, records, result)

            qualification["changed_tree_sha256"] = "0" * 64
            stack_path.write_text(
                json.dumps({"qualification": qualification}), encoding="utf-8"
            )
            with self.assertRaisesRegex(StackError, "changed overlay qualification"):
                patch_stack.validate_qualification(stack_path, records, result)

    def test_worktree_path_digest_covers_untracked_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, _ = self.repository(temporary)
            paths = {Path("a.txt"), Path("b.txt")}
            before = patch_stack.worktree_paths_sha256(root, paths)
            (root / "b.txt").write_text("changed\n", encoding="utf-8")

            self.assertNotEqual(
                before, patch_stack.worktree_paths_sha256(root, paths)
            )

    def test_intermediate_failure_rolls_back_exact_worktree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, base = self.repository(temporary)
            records = self.records(temporary)
            expected_tree = preflight(root, base, records)
            (root / "b.txt").write_text("locally dirty\n", encoding="utf-8")
            status = self.git(root, "status", "--porcelain=v1", "--untracked-files=all")
            before = {name: (root / name).read_bytes()
                      for name in ("ac6recomp_config.toml", "a.txt", "b.txt")}

            with self.assertRaises(StackError):
                apply_stack(root, records, expected_tree)

            self.assertEqual(
                self.git(root, "status", "--porcelain=v1", "--untracked-files=all"),
                status,
            )
            self.assertEqual(
                {name: (root / name).read_bytes()
                 for name in ("ac6recomp_config.toml", "a.txt", "b.txt")},
                before,
            )

    def test_final_tree_mismatch_rolls_back_exact_worktree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, _ = self.repository(temporary)
            records = self.records(temporary)
            status = self.git(root, "status", "--porcelain=v1", "--untracked-files=all")
            before = {name: (root / name).read_bytes()
                      for name in ("ac6recomp_config.toml", "a.txt", "b.txt")}

            with self.assertRaisesRegex(StackError, "runtime overlay tree mismatch"):
                apply_stack(root, records, "0" * 40)

            self.assertEqual(
                self.git(root, "status", "--porcelain=v1", "--untracked-files=all"),
                status,
            )
            self.assertEqual(
                {name: (root / name).read_bytes()
                 for name in ("ac6recomp_config.toml", "a.txt", "b.txt")},
                before,
            )

    def test_concurrent_interference_rolls_back_snapshot_and_index(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, base = self.repository(temporary)
            records = self.records(temporary)
            expected_tree = preflight(root, base, records)
            status = self.git(root, "status", "--porcelain=v1", "--untracked-files=all")
            index_path = Path(self.git(root, "rev-parse", "--git-path", "index"))
            if not index_path.is_absolute():
                index_path = root / index_path
            index_before = index_path.read_bytes()
            before = {name: (root / name).read_bytes()
                      for name in ("ac6recomp_config.toml", "a.txt", "b.txt")}
            original_run_git = patch_stack.run_git
            interfered = False

            def run_with_interference(
                command_root: Path, *arguments: str, **kwargs: object
            ) -> str:
                nonlocal interfered
                if (not interfered and arguments[:2] == ("apply", "--index")
                        and "--check" in arguments
                        and arguments[-1].endswith("2.patch")):
                    interfered = True
                    (root / "b.txt").write_text("concurrent\n", encoding="utf-8")
                return original_run_git(command_root, *arguments, **kwargs)

            with mock.patch.object(
                patch_stack, "run_git", side_effect=run_with_interference
            ), self.assertRaises(StackError):
                apply_stack(
                    root, records, expected_tree,
                    capture_configuration=b"capture\n",
                )

            self.assertTrue(interfered)
            self.assertEqual(index_path.read_bytes(), index_before)
            self.assertEqual(
                self.git(root, "status", "--porcelain=v1", "--untracked-files=all"),
                status,
            )
            self.assertEqual(
                {name: (root / name).read_bytes()
                 for name in ("ac6recomp_config.toml", "a.txt", "b.txt")},
                before,
            )

    def test_success_installs_capture_config_and_unstages_overlay(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root, base = self.repository(temporary)
            records = self.records(temporary)
            expected_tree = preflight(root, base, records)

            apply_stack(
                root, records, expected_tree,
                capture_configuration=b"capture\n",
            )

            self.assertEqual(
                (root / "ac6recomp_config.toml").read_bytes(), b"capture\n"
            )
            self.assertEqual((root / "a.txt").read_text(encoding="utf-8"), "two\n")
            self.assertEqual((root / "b.txt").read_text(encoding="utf-8"), "two\n")
            self.assertEqual(self.git(root, "diff", "--cached", "--name-only"), "")


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
            "language": "PowerPC:BE:64:Xenon",
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

    def test_xenon_generated_source_and_symbol_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "ppc_recomp.0.cpp").write_text(
                "void native_name() {}\n", encoding="utf-8"
            )
            with self.assertRaises(boundary.BoundaryError):
                boundary.audit_source(source)

            (source / "ppc_recomp.0.cpp").unlink()
            (source / "runtime.cpp").write_text(
                "void* table = PPCFuncMappings;\n", encoding="utf-8"
            )
            with self.assertRaises(boundary.BoundaryError):
                boundary.audit_source(source)

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

    def test_package_rejects_xenon_generated_name_and_elf_symbol(self) -> None:
        cases = (
            ("ac6/lib/ppc_recomp.12.cpp", b"generated guest code"),
            ("ac6/bin/ac6-native", b"\x7fELF\x00PPCFuncMappings\x00"),
        )
        for name, payload in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                archive_path = Path(temporary) / "bad.tar.gz"
                with tarfile.open(archive_path, "w:gz") as archive:
                    info = tarfile.TarInfo(name)
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

    def test_package_rejects_unscanned_oversized_member(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            archive_path = Path(temporary) / "bad.tar.gz"
            payload = b"too large"
            with tarfile.open(archive_path, "w:gz") as archive:
                info = tarfile.TarInfo("ac6/share/data.bin")
                info.size = len(payload)
                archive.addfile(info, io.BytesIO(payload))
            saved = sys.argv
            sys.argv = ["audit_native_package.py", str(archive_path)]
            try:
                with mock.patch.object(
                    audit_native_package, "MAX_PACKAGE_MEMBER_BYTES", len(payload) - 1
                ):
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
