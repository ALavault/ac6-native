"""The name universe must come from the guest import table, not from the
native run. Restricting it to names the native side called would filter out
exactly the oracle-only imports the tool exists to find."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/diff_import_sequences.py"

NATIVE = (
    "AC6_IMPORT_CALL tick=0 thread=1 lr=0x1 module=xboxkrnl.exe ordinal=1"
    " name=NtSetEvent r3=0x0\n"
    "AC6_IMPORT_CALL tick=1 thread=1 lr=0x2 module=xam.xex ordinal=2"
    " name=XamUserGetSigninState r3=0x0\n"
)
ORACLE = (
    "i> 0 NtSetEvent(00000000)\n"
    "d> 0 XamUserGetSigninState(00000000)\n"
    "d> 0 XNotifyGetNext(00000001, 00000000)\n"
)


class DiffImportSequencesTests(unittest.TestCase):
    def run_tool(self, names):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "native.log").write_text(NATIVE, encoding="utf-8")
            (root / "oracle.log").write_text(ORACLE, encoding="utf-8")
            (root / "names.txt").write_text("\n".join(names), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(TOOL), str(root / "native.log"),
                 str(root / "oracle.log"), "--names", str(root / "names.txt")],
                check=True, capture_output=True, text=True)
            return result.stdout

    def test_reports_an_import_only_the_oracle_calls(self):
        output = self.run_tool(
            ["NtSetEvent", "XamUserGetSigninState", "XNotifyGetNext"])
        self.assertIn("only_oracle=XNotifyGetNext", output)
        self.assertIn("first_divergence=2", output)
        self.assertIn("native_calls=2", output)

    def test_a_name_outside_the_import_table_is_not_a_divergence(self):
        # Xenia's log carries parenthesised words that are not kernel exports.
        output = self.run_tool(["NtSetEvent", "XamUserGetSigninState"])
        self.assertIn("only_oracle=-", output)
        self.assertIn("first_divergence=none", output)


if __name__ == "__main__":
    unittest.main()
