from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_cpp_complexity import audit, function_sizes


class ComplexityTests(unittest.TestCase):
    def test_counts_physical_lines_and_function(self):
        funcs = function_sizes("int f() {\n // }\n return 1;\n}\n")
        self.assertEqual(funcs[0]["lines"], 4)

    def test_excludes_build_and_allows_growth_within_budget(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "src").mkdir()
            (root / "build-sanitize").mkdir()
            (root / ".build/plan-probes").mkdir(parents=True)
            (root / "sdk").mkdir()
            (root / "src/a.cpp").write_text("int f() { return 1; }\n")
            (root / "build-sanitize/b.cpp").write_text("x\n" * 2000)
            (root / ".build/plan-probes/c.cpp").write_text("x\n" * 2000)
            (root / "sdk/vendor.h").write_text("x\n" * 2000)
            baseline = root / "base.json"
            baseline.write_text(json.dumps({"files": {
                "src/a.cpp": {"kind": "source", "lines": 0, "functions": []}
            }}))
            _, errors = audit(root, baseline)
            self.assertFalse(errors)

    def test_ratchets_only_existing_over_budget_debt(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "src").mkdir()
            path = root / "src/a.cpp"
            path.write_text("x\n" * 1250)
            baseline = root / "base.json"
            baseline.write_text(json.dumps({"files": {
                "src/a.cpp": {"kind": "source", "lines": 1300, "functions": []}
            }}))
            _, errors = audit(root, baseline)
            self.assertFalse(errors)

            path.write_text("x\n" * 1301)
            _, errors = audit(root, baseline)
            self.assertTrue(any("aggravated over-budget baseline" in error for error in errors))

    def test_new_over_budget_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "src").mkdir()
            (root / "src/a.cpp").write_text("x\n" * 1201)
            baseline = root / "base.json"
            baseline.write_text(json.dumps({"files": {}}))
            _, errors = audit(root, baseline)
            self.assertTrue(any("physical lines" in error for error in errors))

    def test_rejects_ambiguous_baseline_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "src").mkdir()
            (root / "src/a.cpp").write_text("int f() { return 1; }\n")
            baseline = root / "base.json"
            baseline.write_text(json.dumps({"files": {
                "src/a.cpp": {"kind": "source", "lines": 1, "functions": []},
                "project/src/a.cpp": {"kind": "source", "lines": 1, "functions": []},
            }}))
            _, errors = audit(root, baseline)
            self.assertTrue(any("duplicate paths" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
