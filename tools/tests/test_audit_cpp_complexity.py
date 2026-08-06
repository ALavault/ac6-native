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
        funcs = function_sizes('int f() {\n // }\n return 1;\n}\n')
        self.assertEqual(funcs[0]["lines"], 4)

    def test_excludes_build_and_data_and_checks_growth(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); (root / "src").mkdir(); (root / "build-sanitize").mkdir()
            (root / "src/a.cpp").write_text("int f() { return 1; }\n")
            (root / "build-sanitize/b.cpp").write_text("x\n" * 2000)
            base = root / "base.json"; base.write_text(json.dumps({"files": {"src/a.cpp": {"kind":"source", "lines":0, "functions":[]}}}))
            _, errors = audit(root, base)
            self.assertTrue(any("aggravated baseline" in error for error in errors))
            self.assertFalse(any("build" in error for error in errors))

    def test_rejects_ambiguous_baseline_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); (root / "src").mkdir()
            (root / "src/a.cpp").write_text("int f() { return 1; }\n")
            base = root / "base.json"
            base.write_text(json.dumps({"files": {
                "src/a.cpp": {"kind": "source", "lines": 1, "functions": []},
                "project/src/a.cpp": {"kind": "source", "lines": 1, "functions": []},
            }}))
            _, errors = audit(root, base)
            self.assertTrue(any("duplicate paths" in error for error in errors))

if __name__ == "__main__": unittest.main()
