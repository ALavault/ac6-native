import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1]))
from extract_ac6_trace_v2_input import extract


class ExtractTraceInputTest(unittest.TestCase):
    def test_extracts_ordered_rows(self) -> None:
        domains = (
            "controller_input", "simulation_snapshot", "mission_objectives",
            "graphics_submission", "output_hashes")
        events = []
        for tick in (1, 2):
            payloads = [
                {"pitch": 1, "roll": -2, "yaw": 3,
                 "throttle": 4, "buttons": 5}, {}, {}, {},
                {"x": "0" * 64},
            ]
            for domain, payload in zip(domains, payloads):
                events.append({"sequence": len(events), "tick": tick,
                               "domain": domain, "payload": payload})
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text("".join(json.dumps(event) + "\n" for event in events))
            self.assertEqual(
                extract(path, 1, 2),
                "1 1 -2 3 4 5\n2 1 -2 3 4 5\n")

    def test_rejects_wrong_tick(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text("{}\n")
            with self.assertRaises(ValueError):
                extract(path, 1, 1)


if __name__ == "__main__":
    unittest.main()
