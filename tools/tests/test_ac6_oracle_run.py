from __future__ import annotations

import importlib.util
import sys
import tempfile
from types import SimpleNamespace
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "ac6-oracle-run.py"
sys.path.insert(0, str(SCRIPT.parent))
SPEC = importlib.util.spec_from_file_location("ac6_oracle_run", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class OracleDisplayTests(unittest.TestCase):
    def test_bare_display_number_is_normalized(self) -> None:
        self.assertEqual(RUNNER.normalize_display("210"), ":210")

    def test_qualified_display_forms_are_preserved(self) -> None:
        self.assertEqual(RUNNER.normalize_display(":210"), ":210")
        self.assertEqual(RUNNER.normalize_display(":210.0"), ":210.0")

    def test_ambiguous_display_is_rejected(self) -> None:
        for value in ("host:210", ":", "210.1", "-1"):
            with self.subTest(value=value), self.assertRaises(RUNNER.RunError):
                RUNNER.normalize_display(value)

    def test_pulse_sequence_is_bounded(self) -> None:
        self.assertEqual(RUNNER.parse_pulse_keys("Escape+space"),
                         ["Escape", "space"])
        for value in ("", "+space", "Escape+bad-key", "a+b+c+d+e"):
            with self.subTest(value=value), self.assertRaises(RUNNER.RunError):
                RUNNER.parse_pulse_keys(value)

    def test_wait_pulse_stops_between_keys_at_guest_boundary(self) -> None:
        runner = RUNNER.OracleRun(SimpleNamespace(duration=10, display=":210"))
        keys: list[str] = []
        reads = iter(("", "type28=30"))
        runner.new_log_text = lambda: next(reads, "")
        runner.input_edge = lambda kind, key, hold: keys.append(key)
        runner.sleep = lambda seconds: None

        runner.wait_log("type28=30", 2, "Escape+space")

        self.assertEqual(keys, ["Escape"])

    def test_sequential_waits_preserve_same_chunk_suffix(self) -> None:
        runner = RUNNER.OracleRun(SimpleNamespace(duration=10, display=":210"))
        reads = iter(("player-ready\nmanager-ready\n", ""))
        runner.new_log_text = lambda: next(reads, "")
        runner.sleep = lambda seconds: None

        runner.wait_log("player-ready", 2)
        runner.wait_log("manager-ready", 2)

        self.assertEqual(runner.pending_log_text, "\n")

    def test_cleanup_removes_only_current_owned_rexglue_segment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            retained = root / "rexglue_memory_100"
            created = root / "rexglue_memory_200"
            unrelated = root / "xenia_memory_300"
            retained.write_bytes(b"old")
            before = RUNNER.shm_inventory(root)
            created.write_bytes(b"new")
            unrelated.write_bytes(b"other")

            cleaned = RUNNER.cleanup_owned_shm(before, root)

            self.assertEqual(cleaned, [{"name": created.name, "bytes": 3}])
            self.assertTrue(retained.exists())
            self.assertFalse(created.exists())
            self.assertTrue(unrelated.exists())

    def test_route_include_is_expanded_and_bounded_to_project(self) -> None:
        with tempfile.TemporaryDirectory(dir=RUNNER.ROOT / "scripts") as temporary:
            root = Path(temporary)
            child = root / "child.steps"
            route = root / "route.steps"
            child.write_text("sleep\t1\n", encoding="utf-8")
            route.write_text("include\tchild.steps\ncapture\tend\n", encoding="utf-8")
            self.assertEqual(
                RUNNER.parse_steps(route),
                [("sleep", "1", ""), ("capture", "end", "")],
            )
            child.write_text("include\troute.steps\n", encoding="utf-8")
            with self.assertRaises(RUNNER.RunError):
                RUNNER.parse_steps(route)

    def test_arm_trace_creates_one_owned_signal(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            runner = RUNNER.OracleRun(
                SimpleNamespace(duration=10, display=":210", output=output)
            )
            runner.execute([("arm-trace", "", "")])
            self.assertTrue((output / "mission01-execution-v2.arm").is_file())
            with self.assertRaises(RUNNER.RunError):
                runner.execute([("arm-trace", "", "")])

    def test_trace_route_rejects_startup_fps_unlock(self) -> None:
        steps = [("sleep", "1", ""), ("arm-trace", "", "")]
        RUNNER.validate_trace_timing(steps, False)
        with self.assertRaisesRegex(RUNNER.RunError, "at arm time"):
            RUNNER.validate_trace_timing(steps, True)

    def write_trace_input(self, root: Path, rows: int = 3600) -> Path:
        path = root / "input.tsv"
        path.write_text(
            "".join(f"{tick} 0 0 0 0 0\n" for tick in range(1, rows + 1)),
            encoding="utf-8",
        )
        return path

    def test_trace_input_requires_exactly_3600_rows(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_trace_input(Path(temporary), 3599)
            with self.assertRaisesRegex(RUNNER.RunError, "3599 rows"):
                RUNNER.validate_trace_input(path)

    def test_trace_input_rejects_out_of_range_value(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_trace_input(Path(temporary))
            lines = path.read_text(encoding="utf-8").splitlines()
            lines[11] = "12 0 0 0 256 0"
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(RUNNER.RunError, "throttle outside bounds"):
                RUNNER.validate_trace_input(path)

    def test_trace_input_rejects_duplicate_tick(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_trace_input(Path(temporary))
            lines = path.read_text(encoding="utf-8").splitlines()
            lines[11] = "11 0 0 0 0 0"
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(RUNNER.RunError, "non-sequential"):
                RUNNER.validate_trace_input(path)

    def test_trace_input_accepts_bounded_sequential_rows(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_trace_input(Path(temporary))
            self.assertEqual(RUNNER.validate_trace_input(path), 3600)

    def test_trace_input_staging_uses_one_validated_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.write_trace_input(root)
            snapshot = RUNNER.load_trace_input_snapshot(source)
            source.write_text("replaced after validation\n", encoding="utf-8")
            runtime = root / "runtime.tsv"

            RUNNER.stage_trace_input(snapshot, runtime)

            self.assertEqual(runtime.read_bytes(), snapshot.payload)
            self.assertEqual(RUNNER.sha256(runtime), snapshot.sha256)
            self.assertEqual(runtime.stat().st_mode & 0o777, 0o444)


if __name__ == "__main__":
    unittest.main()
