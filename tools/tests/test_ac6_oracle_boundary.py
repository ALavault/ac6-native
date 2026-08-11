from __future__ import annotations

import io
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

import audit_ac6_product_boundary as boundary
import audit_native_package
from audit_ac6_oracle_manifest import ManifestError, validate_document
from normalize_ac6_recomp_trace import TraceError, load_events

MANIFEST = ROOT / "analysis/oracle/ac6-recomp-dcd41b/manifest.json"
RAW_TRACE = ROOT / "analysis/oracle/ac6-recomp-dcd41b/fixtures/mission01-frame.raw.jsonl"


class OracleManifestTests(unittest.TestCase):
    def test_versioned_manifest_and_probe_hash_validate(self) -> None:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(validate_document(document, ROOT), 1)

    def test_wrong_oracle_commit_is_rejected(self) -> None:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        document["oracle"]["commit"] = "0" * 40
        with self.assertRaises(ManifestError):
            validate_document(document, ROOT)


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
