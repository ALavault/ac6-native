"""Dependency-free JSON-RPC/MCP stdio façade.

Twelve bounded tools are exposed: six v1 macro tools and six v2 session tools.
The server keeps episode receipts in memory for the lifetime of the process;
it never interprets a host command, opens a caller-provided path, or exposes
guest-memory access.
"""

from __future__ import annotations

import copy
import json
import sys
import uuid
from threading import RLock
from collections import deque
from typing import Any, IO, Mapping

try:
    from ..protocol import canonical_json as _protocol_canonical_json
except ImportError:  # pragma: no cover - standalone copy fallback
    _protocol_canonical_json = None

from ..exploration import (
    ExplorationError,
    branch_episode,
    compare_episodes,
    explore_edge,
    inspect_episode,
    minimize_episode,
    replay,
    run_episode,
    explore_request,
    compare_protocol_results,
    branch_request,
    minimize_request,
)
from ..backends.xenia import capability_matrix
from ..backends.demo_recomp import DemoRecompTransport, DemoRecompTransportError
from ..protocol.v2 import (
    ACTION_SCHEMA, OBSERVATION_SCHEMA, RECEIPT_SCHEMA, TARGET_ID, XEX_SHA256, digest,
    unavailable_observation, validate_action, validate_observation,
    validate_receipt,
)


PROTOCOL_VERSION = "2024-11-05"
SERVER_NAME = "ac6-emu-agent"
SERVER_VERSION = "1.0"
ALLOWED_TOOLS = (
    "emu_capabilities",
    "emu_run_episode",
    "emu_compare_episodes",
    "emu_branch_episode",
    "emu_minimize_reproducer",
    "emu_inspect_artifact",
)
# Short names remain accepted for direct callers that used the first draft of
# this façade, but are not advertised by tools/list and cannot add operations.
TOOL_ALIASES = {
    "run_episode": "emu_run_episode",
    "compare": "emu_compare_episodes",
    "branch": "emu_branch_episode",
    "minimize": "emu_minimize_reproducer",
    "inspect": "emu_inspect_artifact",
}
DENIED_OPERATIONS = (
    "shell",
    "read_arbitrary",
    "read_memory",
    "write_memory",
    "press_button",
)
V2_TOOLS = ("emu_open_session", "emu_step", "emu_observe", "emu_run_until", "emu_replay", "emu_close_session")
V2_MAX_SESSIONS = 64
V2_MAX_ACTIONS = 256
V2_MAX_TICKS = 0xFFFFFFFF
V2_MAX_OBSERVATIONS = 256
V2_MAX_TOMBSTONES = 128


def _json(value: Any) -> str:
    if _protocol_canonical_json is not None:
        return _protocol_canonical_json(value)
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _error(code: int, message: str, request_id: Any = None) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


def _arguments(value: Any) -> dict[str, Any]:
    if value is None:
        return {}
    if not isinstance(value, Mapping):
        raise ExplorationError("tool arguments must be an object")
    return dict(value)


def _reject_unknown_arguments(name: str, arguments: Mapping[str, Any]) -> None:
    allowed = {
        "emu_capabilities": set(),
        "emu_run_episode": {"spec", "actions", "seed", "max_frames", "backend", "mode", "budget"},
        "emu_compare_episodes": {
            "left_episode_id", "right_episode_id", "left_episode", "right_episode", "left", "right", "profile",
        },
        "emu_branch_episode": {
            "episode_id", "from_episode", "artifact_id", "episode", "checkpoint", "actions", "variants", "timeline", "seed", "max_frames",
        },
        "emu_minimize_reproducer": {"episode_id", "from_episode", "artifact_id", "episode", "predicate", "max_attempts"},
        "emu_inspect_artifact": {"episode_id", "from_episode", "artifact_id", "episode", "artifact", "view"},
    }[name]
    unknown = sorted(set(arguments) - allowed)
    if unknown:
        raise ExplorationError("unsupported tool argument(s): " + ", ".join(map(str, unknown)))


def _compare_protocol_results(left: Mapping[str, Any], right: Mapping[str, Any]) -> dict[str, Any]:
    """Compare two emu-agent-result envelopes without exposing payloads."""

    left_frames = left.get("frames", [])
    right_frames = right.get("frames", [])
    differences: list[dict[str, Any]] = []
    for field in ("target", "qualified", "qualification_failures", "stop_reason", "guest_progress", "observer_liveness", "timeline_sha256"):
        if left.get(field) != right.get(field):
            differences.append({"field": field, "left": left.get(field), "right": right.get(field)})
            break
    if not differences:
        for index, (lframe, rframe) in enumerate(zip(left_frames, right_frames)):
            if lframe != rframe:
                differences.append({"field": "frames", "index": index, "left_state_sha256": lframe.get("state_sha256"), "right_state_sha256": rframe.get("state_sha256")})
                break
    if not differences and len(left_frames) != len(right_frames):
        differences.append({"field": "frames.length", "left": len(left_frames), "right": len(right_frames)})
    return {
        "schema": "emu-agent-result-compare/v1",
        "operation": "compare",
        "execution": "simulated",
        "evidence_class": "simulated",
        "equivalent": not differences,
        "left_episode_id": left.get("request_id"),
        "right_episode_id": right.get("request_id"),
        "first_divergence": differences[0] if differences else None,
        "differences": differences,
        "proof": {"simulated": True, "real": False, "needs_dynamic_evidence": True},
    }


def _tool_schema(name: str) -> dict[str, Any]:
    common = {
        "type": "object",
        "additionalProperties": False,
    }
    if name in V2_TOOLS:
        stick = {"type": "object", "additionalProperties": False, "required": ["x", "y"], "properties": {"x": {"type": "integer", "minimum": -32768, "maximum": 32767}, "y": {"type": "integer", "minimum": -32768, "maximum": 32767}}}
        action = {
            "type": "object", "additionalProperties": False,
            "required": ["schema", "action_id", "session_id", "sequence", "tick", "xinput"],
            "properties": {
                "schema": {"const": ACTION_SCHEMA}, "action_id": {"type": "string", "minLength": 1},
                "session_id": {"type": "string", "minLength": 1},
                "sequence": {"type": "integer", "minimum": 0, "maximum": V2_MAX_TICKS},
                "tick": {"type": "integer", "minimum": 0, "maximum": V2_MAX_TICKS},
                "xinput": {
                    "type": "object", "additionalProperties": False,
                    "required": ["buttons", "left_trigger", "right_trigger", "left_stick", "right_stick", "connected"],
                    "properties": {
                        "buttons": {"type": "integer", "minimum": 0, "maximum": 65535},
                        "left_trigger": {"type": "integer", "minimum": 0, "maximum": 255},
                        "right_trigger": {"type": "integer", "minimum": 0, "maximum": 255},
                        "left_stick": {"$ref": "#/$defs/stick"}, "right_stick": {"$ref": "#/$defs/stick"},
                        "connected": {"type": "boolean"},
                    },
                },
            },
        }
        schemas = {
            "emu_open_session": ({"target", "backend"}, {"target": {"type": "object", "additionalProperties": False, "required": ["target_id", "program_sha256", "module"], "properties": {"target_id": {"const": TARGET_ID}, "program_sha256": {"const": XEX_SHA256}, "module": {"const": "Default.xex"}}}, "backend": {"enum": ["demo-recomp", "demo-native"]}}),
            "emu_step": ({"session_id", "action"}, {"session_id": {"type": "string", "minLength": 1}, "action": {"$ref": "#/$defs/action"}}),
            "emu_observe": ({"session_id"}, {"session_id": {"type": "string"}}),
            "emu_run_until": ({"session_id", "actions", "max_steps"}, {"session_id": {"type": "string"}, "actions": {"type": "array", "maxItems": V2_MAX_ACTIONS, "items": {"$ref": "#/$defs/action"}}, "max_steps": {"type": "integer", "minimum": 1, "maximum": V2_MAX_ACTIONS}}),
            "emu_replay": ({"session_id", "receipt_id", "actions"}, {"session_id": {"type": "string"}, "receipt_id": {"type": "string"}, "actions": {"type": "array", "maxItems": V2_MAX_ACTIONS, "items": {"$ref": "#/$defs/action"}}}),
            "emu_close_session": ({"session_id"}, {"session_id": {"type": "string"}}),
        }
        required, properties = schemas[name]
        common["required"] = sorted(required)
        common["properties"] = properties
        common["$defs"] = {"stick": stick, "action": action}
        return common
    if name == "emu_capabilities":
        common["additionalProperties"] = False
    elif name == "emu_run_episode":
        common["properties"] = {
            "spec": {"type": ["string", "object"]},
            "actions": {"type": "array"},
            "seed": {"type": "integer", "minimum": 0},
            "max_frames": {"type": "integer", "minimum": 1},
            "backend": {"enum": ["simulated", "xenia", "real"]},
            "mode": {"enum": ["edge", "replay"]},
            "budget": {"type": "integer", "minimum": 1},
        }
    elif name == "emu_compare_episodes":
        common["properties"] = {
            "left_episode_id": {"type": "string"}, "right_episode_id": {"type": "string"},
            "left_episode": {"type": "object"}, "right_episode": {"type": "object"},
            "left": {"type": "object"}, "right": {"type": "object"},
        }
    elif name == "emu_branch_episode":
        common["properties"] = {
            "episode_id": {"type": "string"}, "from_episode": {"type": "string"},
            "artifact_id": {"type": "string"}, "episode": {"type": "object"},
            "checkpoint": {}, "actions": {"type": "array"}, "seed": {"type": "integer"},
            "max_frames": {"type": "integer"},
        }
    elif name == "emu_minimize_reproducer":
        common["properties"] = {
            "episode_id": {"type": "string"}, "from_episode": {"type": "string"},
            "artifact_id": {"type": "string"}, "episode": {"type": "object"},
            "predicate": {}, "max_attempts": {"type": "integer"},
        }
    elif name == "emu_inspect_artifact":
        common["properties"] = {
            "episode_id": {"type": "string"}, "from_episode": {"type": "string"},
            "artifact_id": {"type": "string"}, "episode": {"type": "object"},
            "artifact": {"type": "object"},
            "view": {"enum": ["summary", "timeline", "transitions", "state", "evidence"]},
        }
    return common


class EmuMcpServer:
    """Stateful in-memory dispatcher for the macro tool set."""

    def __init__(self, *, demo_recomp_binary: str | None = None,
                 demo_native_binary: str | None = None) -> None:
        self._episodes: dict[str, dict[str, Any]] = {}
        self._explorations: dict[str, dict[str, Any]] = {}
        self._v2_sessions: dict[str, dict[str, Any]] = {}
        self._v2_tombstones: deque[dict[str, Any]] = deque(maxlen=V2_MAX_TOMBSTONES)
        self._v2_lock = RLock()
        # Startup configuration only.  Tool arguments can never select a
        # binary, command line, endpoint, or content store.
        self._demo_recomp_binary = demo_recomp_binary
        self._demo_native_binary = demo_native_binary

    @staticmethod
    def _v2_unavailable(session_id: str, sequence: int, *, tick: int,
                        present: int | None, reason: str) -> dict[str, Any]:
        observation = unavailable_observation(session_id, sequence, reason=reason)
        observation["tick"] = tick
        observation["present"] = present
        return validate_observation(observation)

    @staticmethod
    def _v2_transport_failed(session: dict[str, Any]) -> None:
        transport = session.pop("transport", None)
        if isinstance(transport, DemoRecompTransport):
            transport.close()
        session["backend_state"] = "transport_failed"

    def _v2_receipt(self, session_id: str, status: str, *, error: str | None = None, parent: str | None = None) -> dict[str, Any]:
        session = self._v2_sessions.get(session_id, {})
        receipt = {
            "schema": RECEIPT_SCHEMA, "receipt_id": "rcpt-" + uuid.uuid4().hex,
            "session_id": session_id, "status": status, "backend": session.get("backend", "xenia"),
            "qualified": False, "actions_sha256": digest(session.get("actions", [])),
            "observations_sha256": digest(session.get("observations", [])),
            "events": list(session.get("events", [])), "error": error,
            "identity": {"target_id": TARGET_ID, "xex_sha256": XEX_SHA256, "module": "Default.xex"},
        }
        if parent is not None:
            receipt["parent_receipt_id"] = parent
        checked = validate_receipt(receipt)
        session.setdefault("receipt_ids", []).append(checked["receipt_id"])
        session.setdefault("receipt_snapshots", {})[checked["receipt_id"]] = copy.deepcopy({"actions": session.get("actions", []), "observations": session.get("observations", []), "actions_digest": checked["actions_sha256"], "observations_digest": checked["observations_sha256"], "identity": checked["identity"], "owner": session_id})
        return checked

    def _v2_call(self, name: str, args: Mapping[str, Any]) -> dict[str, Any]:
        with self._v2_lock:
            return self._v2_call_unlocked(name, args)

    def _v2_call_unlocked(self, name: str, args: Mapping[str, Any]) -> dict[str, Any]:
        allowed = {
            "emu_open_session": {"target", "backend"}, "emu_step": {"session_id", "action"},
            "emu_observe": {"session_id"}, "emu_run_until": {"session_id", "actions", "max_steps"},
            "emu_replay": {"receipt_id", "session_id", "actions"}, "emu_close_session": {"session_id"},
        }[name]
        unknown = sorted(set(args) - allowed)
        if unknown:
            raise ExplorationError("unsupported v2 argument(s): " + ", ".join(map(str, unknown)))
        if name == "emu_open_session":
            target = args.get("target")
            if len(self._v2_sessions) >= V2_MAX_SESSIONS:
                raise ExplorationError("session limit reached")
            if not isinstance(target, Mapping) or set(target) != {"target_id", "program_sha256", "module"}:
                raise ExplorationError("target must be a bounded AC6 identity object")
            if target.get("target_id") != TARGET_ID or target.get("program_sha256") != XEX_SHA256 or target.get("module") != "Default.xex":
                raise ExplorationError("target identity is not the qualified AC6 demo")
            backend = args.get("backend", "xenia")
            if backend not in {"demo-recomp", "demo-native"}:
                raise ExplorationError("backend_unavailable: backend must be demo-recomp or demo-native")
            session_id = "sess-" + uuid.uuid4().hex
            binary = self._demo_recomp_binary if backend == "demo-recomp" else self._demo_native_binary
            if not binary:
                self._v2_sessions[session_id] = {"backend": backend, "backend_state": "initial_unavailable", "actions": [], "observations": [], "events": [], "receipt_ids": [], "receipt_snapshots": {}, "last_tick": -1, "closed": False}
                return self._v2_receipt(session_id, "backend_unavailable", error=f"{backend} binary is not configured at server startup") | {"session_id": session_id}
            transport: DemoRecompTransport | None = None
            try:
                transport = DemoRecompTransport(binary)
                transport.start()
            except DemoRecompTransportError as error:
                if transport is not None:
                    transport.close()
                self._v2_sessions[session_id] = {"backend": backend, "backend_state": "initial_unavailable", "actions": [], "observations": [], "events": [], "receipt_ids": [], "receipt_snapshots": {}, "last_tick": -1, "closed": False}
                return self._v2_receipt(session_id, "backend_unavailable", error=str(error)) | {"session_id": session_id}
            self._v2_sessions[session_id] = {"backend": backend, "backend_state": "active", "transport": transport, "actions": [], "observations": [], "events": [], "receipt_ids": [], "receipt_snapshots": {}, "last_tick": -1, "closed": False}
            return self._v2_receipt(session_id, "completed") | {"session_id": session_id}
        session_id = args.get("session_id")
        if not isinstance(session_id, str) or session_id not in self._v2_sessions:
            tombstone = next((item for item in self._v2_tombstones if item["session_id"] == session_id), None)
            if name == "emu_close_session" and isinstance(session_id, str) and tombstone is not None:
                return copy.deepcopy(tombstone["receipt"])
            raise ExplorationError("unknown session_id")
        session = self._v2_sessions[session_id]
        if name == "emu_close_session":
            transport = session.get("transport")
            if isinstance(transport, DemoRecompTransport):
                transport.close()
            result = self._v2_receipt(session_id, "closed")
            session["actions"].clear(); session["observations"].clear(); session["events"].clear(); session["receipt_ids"].clear(); session["receipt_snapshots"].clear()
            self._v2_tombstones.append({"session_id": session_id, "receipt": copy.deepcopy(result)})
            self._v2_sessions.pop(session_id, None)
            return result
        if session["closed"]:
            raise ExplorationError("session is closed")
        if session.get("backend_state") == "transport_failed":
            raise ExplorationError("backend_unavailable: session transport failed")
        if name == "emu_observe":
            if len(session["observations"]) >= V2_MAX_OBSERVATIONS:
                raise ExplorationError("observation limit reached")
            transport = session.get("transport")
            if isinstance(transport, DemoRecompTransport):
                try:
                    result = transport.observe()
                    observation = self._v2_unavailable(session_id, len(session["observations"]), tick=result["tick"], present=result["present"], reason=f"unqualified_{session['backend']}_observation")
                except DemoRecompTransportError as error:
                    self._v2_transport_failed(session)
                    raise ExplorationError(str(error))
            else:
                observation = unavailable_observation(session_id, len(session["observations"]))
            session["observations"].append(observation)
            return validate_observation(observation)
        if name == "emu_step":
            action = validate_action(args.get("action"))
            if action["session_id"] != session_id or action["sequence"] != len(session["actions"]):
                raise ExplorationError("action does not belong to session")
            if action["tick"] < session.get("last_tick", -1) or action["tick"] > V2_MAX_TICKS:
                raise ExplorationError("action tick is not monotone")
            if len(session["actions"]) >= V2_MAX_ACTIONS:
                raise ExplorationError("action limit reached")
            if len(session["observations"]) >= V2_MAX_OBSERVATIONS:
                raise ExplorationError("observation limit reached")
            transport = session.get("transport")
            if isinstance(transport, DemoRecompTransport):
                try:
                    result = transport.step(action["xinput"])
                except DemoRecompTransportError as error:
                    self._v2_transport_failed(session)
                    raise ExplorationError(str(error)) from error
                observation = self._v2_unavailable(session_id, len(session["observations"]), tick=result["tick"], present=result["present"], reason=f"unqualified_{session['backend']}_observation")
            else:
                observation = validate_observation(unavailable_observation(session_id, len(session["observations"])))
            session["last_tick"] = action["tick"]
            session["actions"].append(action); session["events"].append({"type": "step", "sequence": action["sequence"]})
            session["observations"].append(observation)
            return {"schema": "emu-agent/v2-step-result", "status": "backend_unavailable", "session_id": session_id, "observation": observation}
        if name == "emu_run_until":
            actions = args.get("actions", [])
            max_steps = args.get("max_steps")
            if not isinstance(max_steps, int) or isinstance(max_steps, bool) or not 1 <= max_steps <= V2_MAX_ACTIONS or not isinstance(actions, list) or len(actions) > V2_MAX_ACTIONS:
                raise ExplorationError("actions must be a bounded array")
            for action in actions[:max_steps]:
                self._v2_call("emu_step", {"session_id": session_id, "action": action})
            if session.get("backend_state") == "active":
                return self._v2_receipt(session_id, "completed")
            return self._v2_receipt(session_id, "backend_unavailable",
                                    error=f"{session['backend']} transport unavailable")
        if name == "emu_replay":
            if "receipt_id" not in args or not isinstance(args.get("receipt_id"), str):
                raise ExplorationError("replay requires a receipt_id")
            snapshot = session.get("receipt_snapshots", {}).get(args["receipt_id"])
            if snapshot is None or snapshot.get("owner") != session_id:
                raise ExplorationError("receipt does not belong to session")
            if "actions" not in args or not isinstance(args["actions"], list):
                raise ExplorationError("replay requires actions from boot")
            if len(args["actions"]) > V2_MAX_ACTIONS or digest(args["actions"]) != snapshot["actions_digest"]:
                raise ExplorationError("replay actions do not match receipt")
            if len(self._v2_sessions) >= V2_MAX_SESSIONS:
                raise ExplorationError("session limit reached")
            replay_session_id = "sess-" + uuid.uuid4().hex
            replay_transport: DemoRecompTransport | None = None
            try:
                if session.get("backend_state") == "active":
                    replay_binary = self._demo_recomp_binary if session["backend"] == "demo-recomp" else self._demo_native_binary
                    if not replay_binary:
                        raise ExplorationError(f"backend_unavailable: {session['backend']} transport is not configured")
                    replay_transport = DemoRecompTransport(replay_binary)
                    replay_transport.start()
                    replay_state = "active"
                else:
                    replay_state = "initial_unavailable"
                self._v2_sessions[replay_session_id] = {"backend": session["backend"], "backend_state": replay_state, "transport": replay_transport, "actions": [], "observations": [], "events": [], "receipt_ids": [], "receipt_snapshots": {}, "last_tick": -1, "closed": False}
                for action in args["actions"]:
                    replay_action = dict(action)
                    replay_action["session_id"] = replay_session_id
                    self._v2_call("emu_step", {"session_id": replay_session_id, "action": replay_action})
                status = "completed" if replay_transport is not None else "backend_unavailable"
                return self._v2_receipt(replay_session_id, status,
                                        error=None if replay_transport is not None else "backend transport unavailable",
                                        parent=args["receipt_id"]) | {"session_id": replay_session_id}
            except Exception as error:
                replay = self._v2_sessions.pop(replay_session_id, None)
                owned = replay.get("transport") if replay is not None else replay_transport
                if isinstance(owned, DemoRecompTransport):
                    owned.close()
                if isinstance(error, ExplorationError):
                    raise
                raise ExplorationError(str(error)) from error
        raise AssertionError(name)

    def capabilities(self) -> dict[str, Any]:
        return {
            "schema": "ac6-emu-agent/capabilities-v1",
            "server": {"name": SERVER_NAME, "version": SERVER_VERSION},
            "transport": {"kind": "mcp-stdio", "jsonrpc": "2.0", "protocol_version": PROTOCOL_VERSION},
            "tools": list(ALLOWED_TOOLS) + list(V2_TOOLS),
            "v2_tools": list(V2_TOOLS),
            "v2_protocol": {
                "action": ACTION_SCHEMA,
                "observation": OBSERVATION_SCHEMA,
                "receipt": RECEIPT_SCHEMA,
            },
            "execution": {
                "simulated": True,
                "real": False,
                "xenia_backend": "oracle-only; not a v2 backend",
                "xenia_transport": False,
                "needs_dynamic_evidence": True,
                "default_backend": "demo-recomp",
            },
            "macro_boundary": {
                "actions": ["advance", "input", "checkpoint", "wait"],
                "controls": [
                    "cancel", "confirm", "down", "fire", "finish", "left", "menu",
                    "mission", "pause", "resume", "retry", "right", "select", "special",
                    "start", "up",
                ],
                "max_actions": 256,
                "max_frames": 120000,
                "xam_input_poll_step": False,
            },
            "denied_operations": list(DENIED_OPERATIONS),
            "evidence": {
                "simulated": "The bundled deterministic model only",
                "real": "No qualified runtime transport is attached",
                "xenia": "Pinned Edge AgentBridge hook exists, but SubmitAction/observation transport is absent",
                "needs_dynamic_evidence": "Required before claiming retail guest state, rendering, or parity",
            },
            "retail_payload_policy": "metadata-only; no retail payload is read or returned",
            "v2_backends": {"demo-recomp": "configured" if self._demo_recomp_binary else "backend_unavailable", "demo-native": "configured" if self._demo_native_binary else "backend_unavailable"},
        }

    def _remember_episode(self, episode: Mapping[str, Any]) -> dict[str, Any]:
        value = copy.deepcopy(dict(episode))
        episode_id = value.get("episode_id", value.get("request_id"))
        if isinstance(episode_id, str):
            self._episodes[episode_id] = value
        return value

    def _resolve_episode(self, arguments: Mapping[str, Any], prefix: str = "episode_id") -> dict[str, Any]:
        key = arguments.get(prefix)
        if key is None and prefix == "episode_id":
            key = arguments.get("from_episode", arguments.get("artifact_id"))
        if isinstance(key, str) and key in self._episodes:
            return copy.deepcopy(self._episodes[key])
        value = arguments.get("episode")
        if isinstance(value, Mapping):
            return copy.deepcopy(dict(value))
        value = arguments.get("artifact")
        if isinstance(value, Mapping):
            return copy.deepcopy(dict(value))
        raise ExplorationError(f"unknown {prefix}")

    def call_tool(self, name: str, arguments: Mapping[str, Any] | None = None) -> dict[str, Any]:
        name = TOOL_ALIASES.get(name, name)
        if name in V2_TOOLS:
            return self._v2_call(name, _arguments(arguments))
        if name not in ALLOWED_TOOLS:
            raise ExplorationError("tool is not exposed by the macro façade")
        args = _arguments(arguments)
        _reject_unknown_arguments(name, args)
        if name == "emu_capabilities":
            if args:
                raise ExplorationError("emu_capabilities takes no arguments")
            return self.capabilities()
        if name == "emu_run_episode":
            # A complete emu-agent/v1 request may be passed directly.  Keep
            # this path metadata-first: the simulated runner is executable,
            # while the PCSX2 adapter returns a structured fail-closed result
            # before any process or guest mutation.
            full_spec = args.get("spec")
            if isinstance(full_spec, Mapping) and full_spec.get("schema") == "emu-agent/v1":
                from ..protocol import load_request
                from ..runner import run_local, run_safe

                request = load_request(full_spec)
                if request["backend"] in {"simulated", "xenia"}:
                    return self._remember_episode(run_local(request))
                return self._remember_episode(run_safe(request))
            mode = args.get("mode")
            if mode == "edge":
                if args.get("backend", "simulated") != "simulated":
                    raise ExplorationError("edge exploration has no attached real backend")
                result = explore_edge(
                    args.get("spec"), seed=args.get("seed", 0), budget=args.get("budget", 16),
                    max_frames=args.get("max_frames", 60),
                )
                self._explorations[result["exploration_id"]] = copy.deepcopy(result)
                for episode in result.get("episodes", []):
                    if isinstance(episode, Mapping):
                        self._remember_episode(episode)
                return result
            if mode == "replay":
                if args.get("backend", "simulated") != "simulated":
                    raise ExplorationError("replay exploration has no attached real backend")
                result = replay(
                    args.get("spec"), args.get("actions"), seed=args.get("seed", 0),
                    max_frames=args.get("max_frames", 600),
                )
                self._explorations[result["exploration_id"]] = copy.deepcopy(result)
                for episode in result.get("episodes", []):
                    if isinstance(episode, Mapping):
                        self._remember_episode(episode)
                return result
            if mode is not None:
                raise ExplorationError("mode must be edge or replay")
            result = run_episode(
                args.get("spec"),
                actions=args.get("actions"),
                seed=args.get("seed", 0),
                max_frames=args.get("max_frames", 600),
                backend=args.get("backend", "simulated"),
            )
            return self._remember_episode(result)
        if name == "emu_compare_episodes":
            left_args = dict(args)
            right_args = dict(args)
            for key in ("left_episode", "left"):
                if key in args:
                    left_args["episode"] = args[key]
                    break
            for key in ("right_episode", "right"):
                if key in args:
                    right_args["episode"] = args[key]
                    break
            left = self._resolve_episode(left_args, "left_episode_id")
            right = self._resolve_episode(right_args, "right_episode_id")
            if left.get("schema") == "emu-agent-result/v1" or right.get("schema") == "emu-agent-result/v1":
                if left.get("schema") != right.get("schema"):
                    raise ExplorationError("cannot compare different receipt schemas")
                comparison = compare_protocol_results(left, right)
                if "profile" in args:
                    comparison["profile"] = args["profile"]
                return comparison
            return compare_episodes(left, right)
        if name == "emu_branch_episode":
            source = self._resolve_episode(args)
            if source.get("schema") == "emu-agent/v1":
                return self._remember_episode(branch_request(
                    source, timeline=args.get("timeline"), seed=args.get("seed")
                ))
            result = branch_episode(
                source,
                checkpoint=args.get("checkpoint", 0),
                actions=args.get("actions", []),
                seed=args.get("seed"),
                max_frames=args.get("max_frames"),
            )
            return self._remember_episode(result)
        if name == "emu_minimize_reproducer":
            source = self._resolve_episode(args)
            if source.get("schema") == "emu-agent/v1":
                return minimize_request(source, max_attempts=args.get("max_attempts", 64))
            result = minimize_episode(
                source,
                predicate=args.get("predicate"),
                max_attempts=args.get("max_attempts", 512),
            )
            episode = result.get("episode")
            if isinstance(episode, Mapping):
                self._remember_episode(episode)
            return result
        if name == "emu_inspect_artifact":
            source = self._resolve_episode(args)
            view = args.get("view", "summary")
            if str(source.get("schema", "")).startswith("ac5-emu-agent/work-manifest"):
                # Manifest inspection is metadata-only and never echoes an
                # arbitrary caller field or a referenced file.
                allowed = (
                    "schema", "evidence_class", "manifest_digest", "git",
                    "pal_target", "pcsx2_appimage_sha256", "config_sha256",
                    "identities", "content_policy",
                )
                inspected = {key: copy.deepcopy(source[key]) for key in allowed if key in source}
                artifact_type = "work-manifest"
            else:
                inspected = inspect_episode(source, view=view)
                artifact_type = "episode-receipt"
            # Keep inspection explicitly metadata-oriented.  Timeline/state
            # views are bounded summaries, never arbitrary guest reads.
            return {
                "schema": "ac5-emu-agent/inspect-v1",
                "operation": "inspect_artifact",
                "artifact_type": artifact_type,
                "artifact_id": source.get("episode_id", source.get("manifest_digest")),
                "view": view,
                "metadata": inspected,
                "evidence_class": source.get("evidence_class", "needs-dynamic-evidence"),
            }
        raise AssertionError(name)

    def _tools_list(self) -> dict[str, Any]:
        names = list(ALLOWED_TOOLS) + list(V2_TOOLS)
        return {
            "tools": [
                {
                    "name": name,
                    "description": (
                        "Bounded macro-level emulator-agent operation; responses label "
                        "simulated versus needs-dynamic-evidence execution."
                    ),
                    "inputSchema": _tool_schema(name),
                }
                for name in names
            ]
        }

    def handle_request(self, request: Any) -> dict[str, Any] | None:
        """Handle one JSON-RPC request; notifications return ``None``."""

        if not isinstance(request, Mapping):
            return _error(-32600, "request must be a JSON object")
        request_id = request.get("id")
        method = request.get("method")
        if not isinstance(method, str) or not method:
            return _error(-32600, "request method is required", request_id)
        is_notification = "id" not in request
        try:
            params = _arguments(request.get("params"))
            if method == "initialize":
                result = {
                    "protocolVersion": PROTOCOL_VERSION,
                    "capabilities": {"tools": {"listChanged": False}},
                    "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
                    "instructions": "Use the six v1 diagnostic tools or six v2 session tools; configured demo-recomp/demo-native transports expose only unqualified observations until guest evidence exists.",
                }
            elif method == "notifications/initialized":
                return None
            elif method == "ping":
                result = {}
            elif method == "tools/list":
                if params:
                    raise ExplorationError("tools/list takes no arguments")
                result = self._tools_list()
            elif method == "tools/call":
                name = params.get("name")
                if not isinstance(name, str):
                    raise ExplorationError("tools/call requires a tool name")
                result = {
                    "content": [],
                    "structuredContent": self.call_tool(name, params.get("arguments")),
                    "isError": False,
                }
                result["content"] = [{"type": "text", "text": _json(result["structuredContent"])}]
            elif method in ALLOWED_TOOLS or method in TOOL_ALIASES or method in V2_TOOLS:
                # Direct operation names make the façade convenient for a
                # small CLI while MCP clients use tools/call above.
                result = self.call_tool(method, params)
            else:
                raise ExplorationError("method is not exposed by the macro façade")
        except ExplorationError as error:
            if is_notification:
                return None
            return _error(-32602, str(error), request_id)
        except (TypeError, ValueError, KeyError) as error:
            if is_notification:
                return None
            return _error(-32602, str(error), request_id)
        if is_notification:
            return None
        return {"jsonrpc": "2.0", "id": request_id, "result": result}


MCPServer = EmuMcpServer


def dispatch(request: Any, server: EmuMcpServer | None = None) -> dict[str, Any] | None:
    """Dispatch one request against a supplied or fresh server."""

    return (server or EmuMcpServer()).handle_request(request)


def serve_stdio(
    input_stream: IO[str] | None = None,
    output_stream: IO[str] | None = None,
    *,
    server: EmuMcpServer | None = None,
) -> None:
    """Serve newline-delimited JSON-RPC until EOF."""

    input_stream = input_stream or sys.stdin
    output_stream = output_stream or sys.stdout
    active = server or EmuMcpServer()
    for line in input_stream:
        if not line.strip():
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError as error:
            response = _error(-32700, "invalid JSON: " + str(error))
        else:
            response = active.handle_request(request)
        if response is not None:
            output_stream.write(_json(response) + "\n")
            output_stream.flush()


__all__ = [
    "ALLOWED_TOOLS",
    "DENIED_OPERATIONS",
    "EmuMcpServer",
    "MCPServer",
    "dispatch",
    "serve_stdio",
]
