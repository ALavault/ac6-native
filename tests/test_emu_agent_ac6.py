from __future__ import annotations

import copy
import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import jsonschema

from tools.emu_agent.backends.xenia import ProfileIsolationError, capability_matrix, copy_isolated_profile
from tools.emu_agent.exploration import branch_request, edge_timeline_variants, explore_request, minimize_request
from tools.emu_agent.mcp_server import EmuMcpServer
from tools.emu_agent.protocol import ValidationError, load_request, unavailable_observation, validate_action, validate_observation, validate_receipt
from tools.emu_agent.mcp_server import server as mcp_server
from tools.emu_agent.runner import run_safe


ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples" / "emu-agent" / "ac6-pitch-replay.json"


def v2_target() -> dict[str, str]:
    return {"target_id": "ac6-demo-xbox360-pal", "program_sha256": "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8", "module": "Default.xex"}


def v2_action(session_id: str, sequence: int) -> dict[str, object]:
    return {"schema": "ac6-agent-action/v1", "action_id": f"a-{sequence}", "session_id": session_id, "sequence": sequence, "tick": sequence, "xinput": {"buttons": 0, "left_trigger": 0, "right_trigger": 0, "left_stick": {"x": 0, "y": 0}, "right_stick": {"x": 0, "y": 0}, "connected": True}}


def qualified_provenance() -> dict[str, str]:
    return {"status": "qualified", "reason": "PAL evidence", "target_id": "ac6-demo-xbox360-pal", "xex_sha256": "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8", "module": "Default.xex", "source_kind": "demo-native", "artifact_sha256": "a" * 64}


class Ac6EmuAgentTests(unittest.TestCase):
    def test_pal_identity_and_controller_timeline_are_canonical(self) -> None:
        request = load_request(EXAMPLE)
        self.assertEqual(request["target"]["program_sha256"], "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8")
        self.assertEqual(request["timeline"][0]["type"], "controller")
        self.assertEqual(request["timeline"][0]["payload"]["duration"]["count"], 240)

    def test_xenia_backend_fails_closed_without_transport(self) -> None:
        request = load_request(EXAMPLE)
        first = run_safe(request)
        second = run_safe(copy.deepcopy(request))
        self.assertEqual(first, second)
        self.assertEqual(first["backend"], "xenia")
        self.assertEqual(first["episode_id"], first["request_id"])
        self.assertEqual(first["status"], "stopped")
        self.assertFalse(first["qualified"])
        self.assertFalse(first["observer_liveness"]["positive_control_seen"])
        self.assertEqual(first["guest_progress"]["end_xam_poll"], 0)
        self.assertIn("xenia-submit-action-transport-unavailable", first["qualification_failures"])

    def test_result_accepts_required_episode_id_alias(self) -> None:
        request = load_request(EXAMPLE)
        result = run_safe(request)
        self.assertEqual(result["episode_id"], "ac6-pitch-replay-neutral")

    def test_safety_and_signed_axis_validation(self) -> None:
        request = load_request(EXAMPLE)
        request["safety"]["allow_shell"] = True
        result = run_safe(request)
        self.assertEqual(result["status"], "error")
        bad = json.loads(EXAMPLE.read_text(encoding="utf-8"))
        bad["timeline"][0]["state"]["lt"] = -1
        with self.assertRaises(ValidationError):
            load_request(bad)

    def test_edge_variants_are_seeded_and_bounded(self) -> None:
        request = load_request(EXAMPLE)
        left = edge_timeline_variants(request, seed=6, budget=8)
        right = edge_timeline_variants(request, seed=6, budget=8)
        other = edge_timeline_variants(request, seed=7, budget=8)
        self.assertEqual(left, right)
        self.assertNotEqual(left, other)
        self.assertEqual(len(left), 8)
        self.assertEqual(left[1]["timeline"][0]["payload"]["state"]["ly"], 1)
        self.assertEqual(left[2]["timeline"][0]["payload"]["state"]["ly"], -1)

    def test_explore_references_results_only(self) -> None:
        request = load_request(EXAMPLE)
        result = explore_request(request, seed=6, budget=8)
        self.assertEqual(result["execution"], "xenia-diagnostic")
        self.assertEqual(result["deduplication"]["input_cases"], 8)
        self.assertTrue(result["proof"]["needs_dynamic_evidence"])

    def test_capability_matrix_and_mcp_surface(self) -> None:
        matrix = capability_matrix()
        self.assertFalse(matrix["capabilities"]["xam_input_poll_step"]["status"] == "available")
        server = EmuMcpServer()
        names = [item["name"] for item in server.handle_request({"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}})["result"]["tools"]]
        self.assertEqual(names, [
            "emu_capabilities", "emu_run_episode", "emu_compare_episodes", "emu_branch_episode",
            "emu_minimize_reproducer", "emu_inspect_artifact", "emu_open_session", "emu_step",
            "emu_observe", "emu_run_until", "emu_replay", "emu_close_session",
        ])
        self.assertNotIn("run_shell", names)

    def test_v2_identity_xinput_and_unavailable_domains(self) -> None:
        server = EmuMcpServer()
        target = {
            "target_id": "ac6-demo-xbox360-pal",
            "program_sha256": "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8",
            "module": "Default.xex",
        }
        opened = server.call_tool("emu_open_session", {"target": target, "backend": "demo-recomp"})
        self.assertEqual(opened["status"], "backend_unavailable")
        self.assertEqual(opened["identity"]["target_id"], target["target_id"])
        session_id = opened["session_id"]
        action = {
            "schema": "ac6-agent-action/v1", "action_id": "a1", "session_id": session_id,
            "sequence": 0, "tick": 0,
            "xinput": {"buttons": 0, "left_trigger": 0, "right_trigger": 0,
                        "left_stick": {"x": 0, "y": 0}, "right_stick": {"x": 0, "y": 0},
                        "connected": True},
        }
        validate_action(action)
        with self.assertRaises(ValidationError):
            validate_action({**action, "command": "shell"})
        self.assertEqual(server.call_tool("emu_step", {"session_id": session_id, "action": action})["status"], "backend_unavailable")
        observation = server.call_tool("emu_observe", {"session_id": session_id})
        self.assertEqual(set(observation["domains"]), {"player", "camera", "flight", "target", "objective", "terminal", "readback"})
        self.assertTrue(all(item["availability"] == "unavailable" and item["value"] is None for item in observation["domains"].values()))

    def test_v2_replay_requires_owned_receipt_and_closes_idempotently(self) -> None:
        server = EmuMcpServer()
        for backend in ("demo-native", "demo-recomp"):
            opened = server.call_tool("emu_open_session", {"target": v2_target(), "backend": backend})
            session_id = opened["session_id"]
            with self.assertRaises(Exception):
                server.call_tool("emu_replay", {"session_id": session_id, "receipt_id": "rcpt-unknown", "actions": []})
            server.call_tool("emu_step", {"session_id": session_id, "action": v2_action(session_id, 0)})
            server.call_tool("emu_observe", {"session_id": session_id})
            closed = server.call_tool("emu_close_session", {"session_id": session_id})
            repeated = server.call_tool("emu_close_session", {"session_id": session_id})
            self.assertEqual(closed["status"], "closed")
            self.assertEqual(repeated, closed)
            self.assertEqual(repeated["backend"], backend)
            repeated["events"].clear()
            self.assertEqual(server.call_tool("emu_close_session", {"session_id": session_id}), closed)

    def test_v2_schemas_are_draft_202012_valid_and_accept_tool_examples(self) -> None:
        server = EmuMcpServer()
        tools = server.handle_request({"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}})["result"]["tools"]
        schemas = {tool["name"]: tool["inputSchema"] for tool in tools if tool["name"].startswith("emu_") and tool["name"] in {"emu_open_session", "emu_step", "emu_observe", "emu_run_until", "emu_replay", "emu_close_session"}}
        self.assertEqual(len(schemas), 6)
        for schema in schemas.values():
            jsonschema.Draft202012Validator.check_schema(schema)
        opened = server.call_tool("emu_open_session", {"target": v2_target(), "backend": "demo-native"})
        session_id = opened["session_id"]
        examples = {
            "emu_open_session": {"target": v2_target(), "backend": "demo-native"},
            "emu_step": {"session_id": session_id, "action": v2_action(session_id, 0)},
            "emu_observe": {"session_id": session_id},
            "emu_run_until": {"session_id": session_id, "actions": [v2_action(session_id, 0)], "max_steps": 1},
            "emu_replay": {"session_id": session_id, "receipt_id": opened["receipt_id"], "actions": []},
            "emu_close_session": {"session_id": session_id},
        }
        for name, example in examples.items():
            jsonschema.validate(example, schemas[name], cls=jsonschema.Draft202012Validator)

    def test_v2_replay_is_owned_immutable_and_max_steps_is_applied(self) -> None:
        server = EmuMcpServer()
        session_id = server.call_tool("emu_open_session", {"target": v2_target(), "backend": "demo-native"})["session_id"]
        actions = [v2_action(session_id, index) for index in range(2)]
        receipt = server.call_tool("emu_run_until", {"session_id": session_id, "actions": actions, "max_steps": 1})
        self.assertEqual(len(server._v2_sessions[session_id]["actions"]), 1)
        saved = copy.deepcopy(server._v2_sessions[session_id]["receipt_snapshots"][receipt["receipt_id"]]["actions"])
        replay = server.call_tool("emu_replay", {"session_id": session_id, "receipt_id": receipt["receipt_id"], "actions": actions[:1]})
        self.assertNotEqual(replay["session_id"], session_id)
        self.assertEqual(server._v2_sessions[session_id]["receipt_snapshots"][receipt["receipt_id"]]["actions"], saved)
        with self.assertRaises(Exception):
            server.call_tool("emu_replay", {"session_id": replay["session_id"], "receipt_id": receipt["receipt_id"], "actions": actions[:1]})

    def test_v2_step_hashes_observation_and_rejects_malformed_provenance(self) -> None:
        server = EmuMcpServer()
        session_id = server.call_tool("emu_open_session", {"target": v2_target(), "backend": "demo-recomp"})["session_id"]
        stepped = server.call_tool("emu_step", {"session_id": session_id, "action": v2_action(session_id, 0)})
        observation = stepped["observation"]
        receipt = server._v2_receipt(session_id, "backend_unavailable", error="x")
        self.assertEqual(receipt["observations_sha256"], mcp_server.digest([observation]))
        malformed = copy.deepcopy(observation)
        malformed["domains"]["player"] = {"availability": "available", "provenance": {"status": "qualified", "reason": "forged"}, "value": "not-a-player"}
        with self.assertRaises(ValidationError):
            validate_observation(malformed)
        forged = copy.deepcopy(receipt); forged["qualified"] = True
        with self.assertRaises(ValidationError):
            validate_receipt(forged)

    def test_v2_available_observation_rejects_unregistered_pal_artifact(self) -> None:
        observation = unavailable_observation("sess-observation")
        observation["availability"] = "available"
        observation["provenance"] = qualified_provenance()
        observation["domains"]["player"] = {"availability": "available", "provenance": qualified_provenance(), "value": {"health": 100.0, "state": "alive"}}
        with self.assertRaises(ValidationError):
            validate_observation(observation)
        malformed = copy.deepcopy(observation)
        malformed["provenance"].pop("artifact_sha256")
        with self.assertRaises(ValidationError):
            validate_observation(malformed)
        malformed = copy.deepcopy(observation)
        malformed["domains"]["player"]["provenance"]["target_id"] = "other"
        with self.assertRaises(ValidationError):
            validate_observation(malformed)
        malformed = copy.deepcopy(observation)
        malformed["provenance"]["source_kind"] = "xenia"
        with self.assertRaises(ValidationError):
            validate_observation(malformed)

    def test_v2_available_camera_and_readback_reject_bool_or_nonfinite_numbers(self) -> None:
        observation = unavailable_observation("sess-values")
        observation["availability"] = "available"
        observation["provenance"] = qualified_provenance()
        observation["domains"]["camera"] = {"availability": "available", "provenance": qualified_provenance(), "value": {"fov": 90.0}}
        with self.assertRaises(ValidationError):
            validate_observation(observation)
        for fov in (True, float("inf"), 180):
            malformed = copy.deepcopy(observation)
            malformed["domains"]["camera"]["value"]["fov"] = fov
            with self.assertRaises(ValidationError):
                validate_observation(malformed)
        observation["domains"]["readback"] = {"availability": "available", "provenance": qualified_provenance(), "value": {"sha256": "b" * 64, "width": 1, "height": 1, "format": "rgba8"}}
        for key in ("width", "height"):
            malformed = copy.deepcopy(observation)
            malformed["domains"]["readback"]["value"][key] = True
            with self.assertRaises(ValidationError):
                validate_observation(malformed)

    def test_v2_resource_limits_and_basic_concurrency(self) -> None:
        with patch.object(mcp_server, "V2_MAX_SESSIONS", 2), patch.object(mcp_server, "V2_MAX_ACTIONS", 2), patch.object(mcp_server, "V2_MAX_OBSERVATIONS", 2), patch.object(mcp_server, "V2_MAX_TOMBSTONES", 2):
            server = EmuMcpServer()
            with ThreadPoolExecutor(max_workers=2) as executor:
                opened = list(executor.map(lambda _: server.call_tool("emu_open_session", {"target": v2_target(), "backend": "demo-native"}), range(2)))
            self.assertEqual(len({item["session_id"] for item in opened}), 2)
            with self.assertRaises(Exception):
                server.call_tool("emu_open_session", {"target": v2_target(), "backend": "demo-native"})
            session_id = opened[0]["session_id"]
            for sequence in range(2):
                server.call_tool("emu_step", {"session_id": session_id, "action": v2_action(session_id, sequence)})
            with self.assertRaises(Exception):
                server.call_tool("emu_step", {"session_id": session_id, "action": v2_action(session_id, 2)})
            server.call_tool("emu_close_session", {"session_id": session_id})
            self.assertEqual(server._v2_tombstones.maxlen, 2)
            self.assertEqual(len(server._v2_tombstones), 1)

    def test_profile_copy_never_overwrites_source_or_destination(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            (source / "Account").write_bytes(b"profile")
            destination = root / "campaign-1"
            receipt = copy_isolated_profile(source, destination, roots=(root,))
            self.assertTrue(receipt["copied"])
            self.assertEqual((source / "Account").read_bytes(), b"profile")
            with self.assertRaises(ProfileIsolationError):
                copy_isolated_profile(source, destination, roots=(root,))

    def test_protocol_branch_and_minimize_remain_diagnostic(self) -> None:
        request = load_request(EXAMPLE)
        branched = branch_request(request, timeline=request["timeline"])
        self.assertEqual(branched["parent_episode_id"], request["request_id"])
        minimized = minimize_request(request)
        self.assertFalse(minimized["preserved"])
        self.assertTrue(minimized["proof"]["needs_dynamic_evidence"])


if __name__ == "__main__":
    unittest.main()
