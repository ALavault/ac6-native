from __future__ import annotations

import importlib.util
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("ac6_vision_control", ROOT / "ac6_vision_control.py")
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def observation(frame: int, visual: str, situation: str = "menu") -> dict:
    return {
        "schema": MODULE.OBSERVATION_SCHEMA,
        "identity": {"xex_sha256": MODULE.TARGET_XEX_SHA256},
        "run_id": "test-run",
        "guest_frame": frame,
        "guest_tick": frame * 2,
        "image": {"path": f"frames/{frame:08d}.png", "sha256": visual * 64},
        "checkpoint": f"checkpoints/{frame:08d}",
        "state": {
            "situation": situation,
            "confidence": 1.0,
            "visual_signature": visual * 64,
            "ocr_text": "TEST",
        },
    }


class VisionControlTests(unittest.TestCase):
    def test_prompt_compiles_to_bounded_menu_explorer(self) -> None:
        options = MODULE.default_options()
        behavior = MODULE.compile_prompt("Explorer volontairement tous les sous-menus sans tirer", options)
        self.assertEqual(behavior.mode, "explore")
        self.assertIn("menu_down", behavior.allowed_options)
        self.assertIn("fire_gun", behavior.blocked_options)
        self.assertNotIn("fire_gun", behavior.forced_options)
        self.assertEqual(behavior.restore_interval, 25)
        behavior.validate(options)

    def test_vision_rules_classify_exact_ocr_and_fail_unknown(self) -> None:
        classifier = MODULE.VisionClassifier(
            [{"situation": "title", "ocr_contains": ["press", "start"], "confidence": 0.95}]
        )
        row = observation(1, "a", "unknown")
        row["state"]["ocr_text"] = "PRESS START"
        classified = classifier.apply(row)
        self.assertEqual(classified["state"]["situation"], "title")
        self.assertEqual(classified["state"]["classifier"], "exact-rules-v1")
        other = observation(2, "b", "unknown")
        self.assertEqual(classifier.apply(other)["state"]["situation"], "unknown")

    def test_observation_identity_and_controller_are_fail_closed(self) -> None:
        parsed = MODULE.Observation.parse(observation(1, "a"))
        self.assertEqual(parsed.situation, "menu")
        wrong = observation(1, "a")
        wrong["identity"]["xex_sha256"] = "0" * 64
        with self.assertRaises(MODULE.ContractError):
            MODULE.Observation.parse(wrong)
        with self.assertRaises(MODULE.ContractError):
            MODULE.Controller(("NOT_A_BUTTON",)).validate()

    def test_bridge_capabilities_are_fail_closed(self) -> None:
        document = {
            "schema": MODULE.BRIDGE_CAPABILITIES_SCHEMA,
            "implementation": {"name": "test-double", "commit": "1" * 40},
            "frame_boundary": "completed_xe_swap",
            "controller_boundary": "guest_xam_poll",
            "capabilities": {
                "pause_after_completed_present": True,
                "exact_guest_frame_step": True,
                "guest_controller_injection": True,
                "framebuffer_capture_while_paused": True,
                "checkpoint_restore": True,
            },
        }
        with tempfile.TemporaryDirectory(dir=os.environ["TMPDIR"]) as directory:
            path = Path(directory) / "capabilities.json"
            path.write_text(json.dumps(document))
            self.assertEqual(MODULE.validate_bridge_capabilities(path), document)
            document["capabilities"]["exact_guest_frame_step"] = False
            path.write_text(json.dumps(document))
            with self.assertRaises(MODULE.ContractError):
                MODULE.validate_bridge_capabilities(path)

    def test_policy_and_archive_are_deterministic(self) -> None:
        behavior = MODULE.Behavior()
        first_archive = MODULE.Archive()
        second_archive = MODULE.Archive()
        first_trace = MODULE.TraceWriter(io.StringIO())
        second_trace = MODULE.TraceWriter(io.StringIO())
        first = MODULE.Engine(behavior, first_archive, 42, first_trace)
        second = MODULE.Engine(behavior, second_archive, 42, second_trace)
        rows = [observation(1, "a"), observation(2, "b"), observation(3, "b")]
        first_actions = [first.step(MODULE.Observation.parse(row)) for row in rows]
        second_actions = [second.step(MODULE.Observation.parse(row)) for row in rows]
        self.assertEqual(first_actions, second_actions)
        self.assertEqual(first_archive.document(), second_archive.document())
        self.assertEqual(len(first_archive.cells), 2)

    def test_strict_action_receipt_matches_exact_frame_advance(self) -> None:
        engine = MODULE.Engine(
            MODULE.Behavior(), MODULE.Archive(), 1, MODULE.TraceWriter(io.StringIO()), strict_receipts=True
        )
        action = engine.step(MODULE.Observation.parse(observation(1, "a")))
        missing = observation(2, "b")
        with self.assertRaises(MODULE.ContractError):
            engine.step(MODULE.Observation.parse(missing))
        received = observation(2, "b")
        received["previous_action"] = {
            "action_id": action["action_id"],
            "advanced_frames": action["hold_frames"],
        }
        engine.step(MODULE.Observation.parse(received))

    def test_forced_behavior_cycles_explicit_options(self) -> None:
        behavior = MODULE.compile_prompt("Déconnexion puis reconnexion", MODULE.default_options())
        trace = MODULE.TraceWriter(io.StringIO())
        engine = MODULE.Engine(behavior, MODULE.Archive(), 1, trace)
        first = engine.step(MODULE.Observation.parse(observation(1, "a")))
        second = engine.step(MODULE.Observation.parse(observation(2, "b")))
        self.assertEqual(first["option"], "disconnect_1")
        self.assertEqual(second["option"], "neutral_1")

    def test_go_explore_requests_bounded_checkpoint_restore(self) -> None:
        behavior = MODULE.Behavior(restore_interval=1)
        archive = MODULE.Archive()
        engine = MODULE.Engine(behavior, archive, 3, MODULE.TraceWriter(io.StringIO()))
        first = engine.step(MODULE.Observation.parse(observation(1, "a")))
        self.assertEqual(first["operation"], "input")
        second = engine.step(MODULE.Observation.parse(observation(2, "b")))
        self.assertEqual(second["operation"], "restore")
        self.assertEqual(second["checkpoint"], "checkpoints/00000001")

    def test_trace_chain_and_movie_replay(self) -> None:
        with tempfile.TemporaryDirectory(dir=os.environ["TMPDIR"]) as directory:
            root = Path(directory)
            trace_path = root / "trace.jsonl"
            archive_path = root / "archive.json"
            with trace_path.open("w", encoding="utf-8") as stream:
                trace = MODULE.TraceWriter(stream)
                engine = MODULE.Engine(MODULE.Behavior(), MODULE.Archive(), 7, trace)
                engine.step(MODULE.Observation.parse(observation(1, "a")))
                engine.step(MODULE.Observation.parse(observation(2, "b")))
                MODULE.atomic_write(archive_path, engine.archive.document())
            records = MODULE.validate_trace(trace_path)
            self.assertEqual([row["kind"] for row in records], ["decision", "feedback", "decision"])
            archive = MODULE.Archive.load(archive_path)
            self.assertEqual(len(archive.cells), 2)

    def test_tampered_trace_is_rejected(self) -> None:
        stream = io.StringIO()
        trace = MODULE.TraceWriter(stream)
        trace.append("session", {"value": 1})
        row = json.loads(stream.getvalue())
        row["payload"]["value"] = 2
        with tempfile.TemporaryDirectory(dir=os.environ["TMPDIR"]) as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text(json.dumps(row) + "\n")
            with self.assertRaises(MODULE.ContractError):
                MODULE.validate_trace(path)


if __name__ == "__main__":
    unittest.main()
