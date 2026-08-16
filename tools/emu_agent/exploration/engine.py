"""Bounded deterministic episode model used by the emulator-agent façade.

This module is intentionally a model, not a claim about retail execution.  It
gives MCP clients a stable contract to test against while making the evidence
class explicit.  No function in this file accesses the filesystem, starts a
process, or reads/writes guest memory.
"""

from __future__ import annotations

import copy
import hashlib
import json
import random
from typing import Any, Iterable, Mapping, Sequence

try:
    # Reuse the repository's visible emu-agent canonical wire helpers.  The
    # fallback keeps this small model importable in isolation for tooling.
    from ..protocol import canonical_json as _protocol_canonical_json
    from ..protocol import sha256_json as _protocol_sha256_json
except ImportError:  # pragma: no cover - only used when copied standalone
    _protocol_canonical_json = None
    _protocol_sha256_json = None


SCHEMA = "ac5-emu-agent/v1"
SIMULATED = "simulated"
REAL = "real"
NEEDS_DYNAMIC = "needs-dynamic-evidence"

MAX_ACTIONS = 256
MAX_FRAMES = 120_000
MAX_STEP_FRAMES = 2_000
MAX_EXPLORATION_CASES = 128
MAX_MINIMIZE_ATTEMPTS = 512

# These are macro labels.  They are not a raw pad/register API.  Keeping the
# list finite also prevents a client from smuggling a command or a path in an
# action name.
SAFE_CONTROLS = frozenset(
    {
        "confirm",
        "cancel",
        "start",
        "select",
        "up",
        "down",
        "left",
        "right",
        "pause",
        "resume",
        "retry",
        "finish",
        "mission",
        "menu",
        "fire",
        "special",
    }
)
SAFE_ACTIONS = frozenset({"advance", "input", "checkpoint", "wait"})
_DENIED_TOKENS = frozenset(
    {
        "shell",
        "read_memory",
        "write_memory",
        "press_button",
        "read_arbitrary",
    }
)
_PHASES = ("boot", "menu", "briefing", "mission", "paused", "complete")
_MAX_STRING = 128


class ExplorationError(ValueError):
    """Raised when a request is outside the bounded macro contract."""


def _canonical(value: Any) -> str:
    if _protocol_canonical_json is not None:
        try:
            return _protocol_canonical_json(value)
        except Exception as error:
            raise ExplorationError("value is not canonical JSON") from error
    try:
        return json.dumps(
            value,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            separators=(",", ":"),
        )
    except (TypeError, ValueError) as error:
        raise ExplorationError("value is not canonical JSON") from error


def canonical_digest(value: Any) -> str:
    """Return the SHA-256 of canonical JSON without exposing host paths."""

    if _protocol_sha256_json is not None:
        try:
            return _protocol_sha256_json(value)
        except Exception as error:
            raise ExplorationError("value is not canonical JSON") from error
    return hashlib.sha256(_canonical(value).encode("utf-8")).hexdigest()


def _int(value: Any, name: str, *, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ExplorationError(f"{name} must be an integer")
    if value < minimum or value > maximum:
        raise ExplorationError(f"{name} is outside the bounded range")
    return value


def _text(value: Any, name: str, *, maximum: int = _MAX_STRING) -> str:
    if not isinstance(value, str) or not value or len(value) > maximum:
        raise ExplorationError(f"{name} must be a non-empty short string")
    if "\x00" in value or "/" in value or "\\" in value or ".." in value:
        raise ExplorationError(f"{name} is not a safe identifier")
    return value


def _mapping(value: Any, name: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ExplorationError(f"{name} must be an object")
    return value


def _safe_scalar(value: Any, name: str) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        if isinstance(value, float) and (value != value or value in (float("inf"), float("-inf"))):
            raise ExplorationError(f"{name} contains a non-finite number")
        if isinstance(value, str) and len(value) > _MAX_STRING:
            raise ExplorationError(f"{name} is too long")
        return value
    raise ExplorationError(f"{name} must be a scalar")


def normalize_spec(spec: Any = None) -> dict[str, Any]:
    """Normalize a scenario descriptor; a string is an identifier, not a path."""

    if spec is None:
        return {"scenario": "unspecified"}
    if isinstance(spec, str):
        return {"scenario": _text(spec, "scenario")}
    value = _mapping(spec, "spec")
    allowed = {"scenario", "target", "identity", "initial_state", "metadata"}
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ExplorationError("spec contains unsupported fields: " + ", ".join(map(str, unknown)))
    scenario = value.get("scenario", value.get("target", "unspecified"))
    normalized: dict[str, Any] = {"scenario": _text(scenario, "spec.scenario")}
    if "target" in value:
        normalized["target"] = _text(value["target"], "spec.target")
    if "identity" in value:
        identity = _mapping(value["identity"], "spec.identity")
        normalized["identity"] = {
            str(key): _safe_scalar(item, f"spec.identity.{key}")
            for key, item in sorted(identity.items(), key=lambda pair: str(pair[0]))
        }
    if "initial_state" in value:
        initial = _mapping(value["initial_state"], "spec.initial_state")
        normalized["initial_state"] = {
            str(key): _safe_scalar(item, f"spec.initial_state.{key}")
            for key, item in sorted(initial.items(), key=lambda pair: str(pair[0]))
        }
    if "metadata" in value:
        metadata = _mapping(value["metadata"], "spec.metadata")
        normalized["metadata"] = {
            str(key): _safe_scalar(item, f"spec.metadata.{key}")
            for key, item in sorted(metadata.items(), key=lambda pair: str(pair[0]))
        }
    return normalized


def _normalize_buttons(value: Any) -> list[str]:
    if value is None:
        return []
    if not isinstance(value, (list, tuple)):
        raise ExplorationError("input.buttons must be an array")
    result: list[str] = []
    for button in value:
        label = _text(button, "input button", maximum=32).lower()
        if label not in SAFE_CONTROLS:
            raise ExplorationError("input button is outside the macro allowlist")
        if label not in result:
            result.append(label)
    return sorted(result)


def _normalize_axes(value: Any) -> dict[str, int]:
    if value is None:
        return {}
    axes = _mapping(value, "input.axes")
    result: dict[str, int] = {}
    for key, raw in sorted(axes.items(), key=lambda pair: str(pair[0])):
        name = _text(str(key), "input axis", maximum=32).lower()
        # Named, normalized axes keep this API macro-level; arbitrary memory
        # offsets and register names can never enter the request.
        if name not in {"pitch", "roll", "yaw", "throttle"}:
            raise ExplorationError("input axis is outside the macro allowlist")
        result[name] = _int(raw, "input axis value", minimum=-32768, maximum=32767)
    return result


def normalize_actions(actions: Any = None) -> list[dict[str, Any]]:
    """Validate and canonicalize macro actions."""

    if actions is None:
        return []
    if not isinstance(actions, (list, tuple)):
        raise ExplorationError("actions must be an array")
    if len(actions) > MAX_ACTIONS:
        raise ExplorationError("actions exceed the bounded episode limit")
    result: list[dict[str, Any]] = []
    for index, raw in enumerate(actions):
        action = _mapping(raw, f"actions[{index}]")
        if any(str(key).lower() in _DENIED_TOKENS for key in action):
            raise ExplorationError("action requests a denied operation")
        op = action.get("op", action.get("kind"))
        op = _text(op, f"actions[{index}].op", maximum=32).lower()
        if op not in SAFE_ACTIONS:
            raise ExplorationError("action is outside the macro allowlist")
        unknown = set(action) - {"op", "kind", "frames", "buttons", "axes", "name"}
        if unknown:
            raise ExplorationError(
                f"actions[{index}] contains unsupported fields: "
                + ", ".join(map(str, sorted(unknown)))
            )
        if op in {"advance", "wait"}:
            frames = _int(action.get("frames", 1), f"actions[{index}].frames", minimum=1, maximum=MAX_STEP_FRAMES)
            result.append({"op": op, "frames": frames})
        elif op == "input":
            buttons = _normalize_buttons(action.get("buttons"))
            axes = _normalize_axes(action.get("axes"))
            if not buttons and not axes:
                raise ExplorationError(f"actions[{index}].input is empty")
            # A zero-duration input is a useful, deterministic t-1 edge case:
            # the controller packet is materialized at the boundary without
            # advancing the guest clock.  Time-advancing actions remain
            # strictly positive.
            frames = _int(action.get("frames", 1), f"actions[{index}].frames", minimum=0, maximum=MAX_STEP_FRAMES)
            result.append({"op": op, "buttons": buttons, "axes": axes, "frames": frames})
        else:
            name = _text(action.get("name"), f"actions[{index}].name", maximum=64)
            result.append({"op": op, "name": name})
    return result


def _identity(spec: Mapping[str, Any]) -> dict[str, Any]:
    identity = copy.deepcopy(spec.get("identity", {}))
    if not isinstance(identity, dict):
        identity = {}
    identity.setdefault("target", spec.get("target", spec.get("scenario", "unqualified")))
    # These values are intentionally absent by default.  The simulator must
    # not imply that a retail ELF or asset was loaded.
    identity.setdefault("elf_sha256", None)
    identity.setdefault("image_base", None)
    return {str(key): identity[key] for key in sorted(identity)}


def _state_fingerprint(state: Mapping[str, Any]) -> str:
    return canonical_digest(state)


def _edge_fingerprint(source: str, event: str, target: str) -> str:
    return canonical_digest({"source": source, "event": event, "target": target})


def _phase_after_input(phase: str, buttons: Sequence[str]) -> str:
    selected = set(buttons)
    if "pause" in selected and phase not in {"complete", "paused"}:
        return "paused"
    if "resume" in selected and phase == "paused":
        return "mission"
    if "cancel" in selected or "menu" in selected:
        return "menu"
    if "finish" in selected:
        return "complete"
    if "retry" in selected:
        return "mission"
    if "start" in selected or "mission" in selected:
        return "mission"
    if "confirm" in selected:
        return {"boot": "menu", "menu": "briefing", "briefing": "mission"}.get(phase, phase)
    return phase


def _initial_state(spec: Mapping[str, Any], seed: int) -> dict[str, Any]:
    initial = {
        "phase": "boot",
        "frame": 0,
        "cursor": 0,
        "score": 0,
        "flags": [],
        # A seed marker makes the simulation's seeded nature visible without
        # pretending it came from guest memory.
        "seed_marker": hashlib.sha256(str(seed).encode("ascii")).hexdigest()[:12],
    }
    provided = spec.get("initial_state", {})
    if isinstance(provided, Mapping):
        for key in ("phase", "cursor", "score"):
            if key in provided:
                initial[key] = provided[key]
        if "phase" in initial and initial["phase"] not in _PHASES:
            raise ExplorationError("spec.initial_state.phase is unsupported")
        initial["cursor"] = _int(initial["cursor"], "initial cursor", minimum=-32768, maximum=32768)
        initial["score"] = _int(initial["score"], "initial score", minimum=-1_000_000, maximum=1_000_000)
    return initial


def _backend_status(backend: str) -> dict[str, Any]:
    if backend == "simulated":
        return {
            "kind": "simulator",
            "execution": SIMULATED,
            "real_runtime": REAL,
            "real_runtime_status": NEEDS_DYNAMIC,
            "retail_payload_loaded": False,
        }
    if backend == "real":
        return {
            "kind": "qualified-runtime",
            "execution": NEEDS_DYNAMIC,
            "real_runtime": REAL,
            "real_runtime_status": NEEDS_DYNAMIC,
            "retail_payload_loaded": False,
            "reason": "No qualified runtime transport is attached to this façade",
        }
    raise ExplorationError("backend must be simulated or real")


def run_episode(
    spec: Any = None,
    *,
    actions: Any = None,
    seed: int = 0,
    max_frames: int = 600,
    backend: str = "simulated",
) -> dict[str, Any]:
    """Run a bounded deterministic model episode.

    ``backend='real'`` is fail-closed and returns a needs-dynamic-evidence
    receipt rather than manufacturing a retail observation.
    """

    normalized_spec = normalize_spec(spec)
    normalized_actions = normalize_actions(actions)
    seed = _int(seed, "seed", minimum=0, maximum=(1 << 64) - 1)
    max_frames = _int(max_frames, "max_frames", minimum=1, maximum=MAX_FRAMES)
    backend = _text(backend, "backend", maximum=16).lower()
    status = _backend_status(backend)
    request_identity = {
        "spec": normalized_spec,
        "actions": normalized_actions,
        "seed": seed,
        "max_frames": max_frames,
        "backend": backend,
    }
    episode_id = canonical_digest(request_identity)[:24]
    base = {
        "schema": SCHEMA,
        "operation": "run_episode",
        "episode_id": episode_id,
        "execution": status["execution"],
        "evidence_class": status["execution"],
        "backend": status,
        "identity": _identity(normalized_spec),
        "spec": normalized_spec,
        "seed": seed,
        "max_frames": max_frames,
        "actions": normalized_actions,
    }
    if backend == "real":
        base.update(
            {
                "status": "not-run",
                "reason": status["reason"],
                "timeline": [],
                "transitions": [],
                "final_state": None,
                "proof": {
                    "simulated": False,
                    "real": False,
                    "needs_dynamic_evidence": True,
                },
            }
        )
        base["digest"] = canonical_digest(base)
        return base

    state = _initial_state(normalized_spec, seed)
    timeline: list[dict[str, Any]] = []
    transitions: list[dict[str, Any]] = []
    frame = 0
    rng = random.Random(seed)
    for index, action in enumerate(normalized_actions):
        before = copy.deepcopy(state)
        source_phase = str(state["phase"])
        op = action["op"]
        frames = int(action.get("frames", 0))
        if frame + frames > max_frames:
            raise ExplorationError("episode exceeds max_frames")
        frame += frames
        state["frame"] = frame
        event = op
        if op == "input":
            buttons = action["buttons"]
            axes = action["axes"]
            state["phase"] = _phase_after_input(source_phase, buttons)
            if "up" in buttons:
                state["cursor"] -= 1
            if "down" in buttons:
                state["cursor"] += 1
            state["cursor"] = max(-32768, min(32768, int(state["cursor"])))
            if "fire" in buttons:
                state["score"] += 1
            if "special" in buttons:
                state["score"] += 5
            if axes:
                state["axis_checksum"] = canonical_digest(axes)[:12]
            event = "input:" + "+".join(buttons or sorted(axes))
        elif op == "checkpoint":
            state["checkpoint"] = action["name"]
            event = "checkpoint:" + action["name"]
        elif op == "wait":
            event = "wait"
        elif op == "advance":
            if source_phase == "boot":
                state["phase"] = "menu"
            event = "advance"
        # A deterministic, model-only token makes different seeds observable
        # in replay output while retaining a stable state shape.
        state["edge_token"] = f"{rng.getrandbits(32):08x}"
        target_phase = str(state["phase"])
        source_fp = _state_fingerprint(before)
        target_fp = _state_fingerprint(state)
        edge = {
            "index": index,
            "frame": frame,
            "source": source_phase,
            "event": event,
            "target": target_phase,
            "source_state": source_fp,
            "target_state": target_fp,
            "fingerprint": _edge_fingerprint(source_phase, event, target_phase),
        }
        transitions.append(edge)
        timeline.append(
            {
                "index": index,
                "frame": frame,
                "action": copy.deepcopy(action),
                "event": event,
                "state": copy.deepcopy(state),
                "state_fingerprint": target_fp,
                "edge_fingerprint": edge["fingerprint"],
            }
        )

    final_state = copy.deepcopy(state)
    base.update(
        {
            "status": "completed",
            "timeline": timeline,
            "observations": timeline,
            "transitions": transitions,
            "final_state": final_state,
            "frames": frame,
            "proof": {
                "simulated": True,
                "real": False,
                "needs_dynamic_evidence": True,
            },
            "state_digest": _state_fingerprint(final_state),
        }
    )
    base["digest"] = canonical_digest(base)
    return base


def _episode_actions(episode: Mapping[str, Any]) -> list[dict[str, Any]]:
    return normalize_actions(episode.get("actions", []))


def _episode_spec(episode: Mapping[str, Any]) -> dict[str, Any]:
    return normalize_spec(episode.get("spec"))


def compare_episodes(left: Mapping[str, Any], right: Mapping[str, Any]) -> dict[str, Any]:
    """Compare observations, retaining evidence provenance and first drift."""

    if not isinstance(left, Mapping) or not isinstance(right, Mapping):
        raise ExplorationError("compare inputs must be episode objects")
    fields = ("identity", "seed", "actions", "frames", "final_state", "transitions")
    differences: list[dict[str, Any]] = []
    for field in fields:
        if left.get(field) != right.get(field):
            differences.append({"field": field, "left": left.get(field), "right": right.get(field)})
    left_class = left.get("evidence_class", NEEDS_DYNAMIC)
    right_class = right.get("evidence_class", NEEDS_DYNAMIC)
    evidence_class = SIMULATED if left_class == right_class == SIMULATED else NEEDS_DYNAMIC
    return {
        "schema": SCHEMA,
        "operation": "compare",
        "execution": evidence_class,
        "evidence_class": evidence_class,
        "equivalent": not differences,
        "left_episode_id": left.get("episode_id"),
        "right_episode_id": right.get("episode_id"),
        "first_difference": differences[0] if differences else None,
        "differences": differences,
        "proof": {
            "simulated": evidence_class == SIMULATED,
            "real": left_class == right_class == REAL,
            "needs_dynamic_evidence": evidence_class == NEEDS_DYNAMIC,
        },
    }


def _resolve_checkpoint(actions: Sequence[Mapping[str, Any]], checkpoint: Any) -> int:
    if isinstance(checkpoint, bool):
        raise ExplorationError("branch checkpoint is invalid")
    if isinstance(checkpoint, int):
        return _int(checkpoint, "branch checkpoint", minimum=0, maximum=len(actions))
    name = _text(checkpoint, "branch checkpoint", maximum=64)
    for index, action in enumerate(actions):
        if action.get("op") == "checkpoint" and action.get("name") == name:
            return index + 1
    raise ExplorationError("branch checkpoint was not found")


def branch_episode(
    episode: Mapping[str, Any],
    *,
    checkpoint: Any = 0,
    actions: Any = None,
    seed: int | None = None,
    max_frames: int | None = None,
) -> dict[str, Any]:
    """Replay an episode prefix and append a bounded macro suffix."""

    if not isinstance(episode, Mapping):
        raise ExplorationError("branch source must be an episode object")
    source_actions = _episode_actions(episode)
    split = _resolve_checkpoint(source_actions, checkpoint)
    suffix = normalize_actions(actions or [])
    branch_actions = source_actions[:split] + suffix
    result = run_episode(
        _episode_spec(episode),
        actions=branch_actions,
        seed=episode.get("seed", 0) if seed is None else seed,
        max_frames=episode.get("max_frames", 600) if max_frames is None else max_frames,
        backend="simulated" if episode.get("execution") == SIMULATED else "real",
    )
    result.update(
        {
            "operation": "branch",
            "parent_episode_id": episode.get("episode_id"),
            "branch_point": split,
            "branch_suffix": suffix,
        }
    )
    result["digest"] = canonical_digest(result)
    return result


def _predicate_holds(episode: Mapping[str, Any], predicate: Any) -> bool:
    if predicate is None:
        predicate = {"type": "transition"}
    if isinstance(predicate, str):
        predicate = {"type": "event", "name": predicate}
    predicate = _mapping(predicate, "predicate")
    kind = _text(predicate.get("type", "transition"), "predicate.type", maximum=32).lower()
    timeline = episode.get("timeline", [])
    transitions = episode.get("transitions", [])
    if kind == "transition":
        return bool(transitions)
    if kind == "event":
        name = _text(predicate.get("name"), "predicate.name", maximum=128)
        return any(item.get("event") == name for item in timeline)
    if kind in {"phase", "final_state"}:
        field = "phase" if kind == "phase" else _text(predicate.get("field"), "predicate.field", maximum=64)
        expected = _safe_scalar(predicate.get("equals"), "predicate.equals")
        final = episode.get("final_state") or {}
        return final.get(field) == expected
    if kind == "edge":
        expected = _text(predicate.get("fingerprint"), "predicate.fingerprint", maximum=128)
        return any(item.get("fingerprint") == expected for item in transitions)
    raise ExplorationError("predicate type is outside the bounded allowlist")


def minimize_episode(
    episode: Mapping[str, Any],
    *,
    predicate: Any = None,
    max_attempts: int = MAX_MINIMIZE_ATTEMPTS,
) -> dict[str, Any]:
    """Delta-debug action macros while preserving a safe built-in predicate."""

    if not isinstance(episode, Mapping):
        raise ExplorationError("minimize source must be an episode object")
    max_attempts = _int(max_attempts, "max_attempts", minimum=1, maximum=MAX_MINIMIZE_ATTEMPTS)
    original_actions = _episode_actions(episode)
    spec = _episode_spec(episode)
    seed = _int(episode.get("seed", 0), "episode.seed", minimum=0, maximum=(1 << 64) - 1)
    frame_limit = _int(episode.get("max_frames", 600), "episode.max_frames", minimum=1, maximum=MAX_FRAMES)

    baseline = run_episode(spec, actions=original_actions, seed=seed, max_frames=frame_limit)
    preserved = _predicate_holds(baseline, predicate)
    candidate = list(original_actions)
    attempts = 0
    if preserved:
        granularity = 2
        while len(candidate) >= 2 and attempts < max_attempts:
            chunk_size = max(1, (len(candidate) + granularity - 1) // granularity)
            reduced = False
            start = 0
            while start < len(candidate) and attempts < max_attempts:
                trial = candidate[:start] + candidate[start + chunk_size :]
                attempts += 1
                trial_episode = run_episode(spec, actions=trial, seed=seed, max_frames=frame_limit)
                if _predicate_holds(trial_episode, predicate):
                    candidate = trial
                    granularity = max(2, granularity - 1)
                    reduced = True
                    break
                start += chunk_size
            if not reduced:
                if granularity >= len(candidate):
                    break
                granularity = min(len(candidate), granularity * 2)

    minimized = run_episode(spec, actions=candidate, seed=seed, max_frames=frame_limit)
    result = {
        "schema": SCHEMA,
        "operation": "minimize",
        "execution": minimized["execution"],
        "evidence_class": minimized["evidence_class"],
        "original_episode_id": episode.get("episode_id"),
        "original_actions": original_actions,
        "minimized_actions": candidate,
        "original_count": len(original_actions),
        "minimized_count": len(candidate),
        "attempts": attempts,
        "preserved": preserved and _predicate_holds(minimized, predicate),
        "predicate": {"type": "transition"} if predicate is None else predicate,
        "episode": minimized,
        "proof": minimized.get("proof", {}),
    }
    result["digest"] = canonical_digest(result)
    return result


def _unique(values: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        if value not in seen:
            seen.add(value)
            result.append(value)
    return result


def _edge_candidates(seed: int, budget: int) -> list[list[dict[str, Any]]]:
    rng = random.Random(seed)
    # Sort before seeding: set iteration is process-hash dependent and would
    # otherwise make two invocations with the same seed disagree.
    controls = sorted(SAFE_CONTROLS)
    rng.shuffle(controls)
    candidates: list[list[dict[str, Any]]] = [
        [{"op": "advance", "frames": 1}],
        [{"op": "wait", "frames": 1}],
        # Deliberate duplicate is useful for proving the deduplication receipt
        # in a bounded smoke exploration.
        [{"op": "wait", "frames": 1}],
    ]
    candidates.extend([[{"op": "input", "buttons": [control], "frames": 1}] for control in controls])
    # A few two-step edges exercise order sensitivity.  Their order is seeded,
    # but every candidate remains a stable macro request.
    pairs = [(controls[i], controls[(i + 1) % len(controls)]) for i in range(len(controls))]
    rng.shuffle(pairs)
    candidates.extend(
        [
            [
                {"op": "input", "buttons": [first], "frames": 1},
                {"op": "input", "buttons": [second], "frames": 1},
            ]
            for first, second in pairs
        ]
    )
    return candidates[:budget]


def explore(
    spec: Any = None,
    *,
    mode: str = "edge",
    seed: int = 0,
    actions: Any = None,
    budget: int = 16,
    max_frames: int = 60,
) -> dict[str, Any]:
    """Explore seeded edge candidates or replay one supplied trace."""

    mode = _text(mode, "mode", maximum=16).lower()
    if mode not in {"edge", "replay"}:
        raise ExplorationError("mode must be edge or replay")
    seed = _int(seed, "seed", minimum=0, maximum=(1 << 64) - 1)
    budget = _int(budget, "budget", minimum=1, maximum=MAX_EXPLORATION_CASES)
    max_frames = _int(max_frames, "max_frames", minimum=1, maximum=MAX_FRAMES)
    normalized_spec = normalize_spec(spec)
    if mode == "replay":
        candidates = [normalize_actions(actions)]
    else:
        candidates = [normalize_actions(candidate) for candidate in _edge_candidates(seed, budget)]

    state_seen: set[str] = set()
    edge_seen: set[str] = set()
    state_owner: dict[str, str] = {}
    edge_owner: dict[str, str] = {}
    cases: list[dict[str, Any]] = []
    unique_episodes: list[dict[str, Any]] = []
    duplicate_cases = 0
    for index, candidate in enumerate(candidates[:budget]):
        episode = run_episode(normalized_spec, actions=candidate, seed=seed, max_frames=max_frames)
        episode_state = [item["state_fingerprint"] for item in episode["timeline"]]
        episode_edges = [item["fingerprint"] for item in episode["transitions"]]
        new_states = [item for item in episode_state if item not in state_seen]
        new_edges = [item for item in episode_edges if item not in edge_seen]
        # Retain the first replay even when it has no observations; subsequent
        # identical empty traces are the duplicates.
        duplicate = bool(unique_episodes) and not new_states and not new_edges
        if duplicate:
            duplicate_cases += 1
            duplicate_of = next(
                (state_owner[item] for item in episode_state if item in state_owner),
                next((edge_owner[item] for item in episode_edges if item in edge_owner), None),
            )
        else:
            duplicate_of = None
            unique_episodes.append(episode)
        state_seen.update(episode_state)
        edge_seen.update(episode_edges)
        for item in episode_state:
            state_owner.setdefault(item, f"case-{index:03d}")
        for item in episode_edges:
            edge_owner.setdefault(item, f"case-{index:03d}")
        cases.append(
            {
                "case_id": f"case-{index:03d}",
                "episode_id": episode["episode_id"],
                "actions": candidate,
                "duplicate": duplicate,
                "duplicate_of": duplicate_of,
                "new_state_fingerprints": new_states,
                "new_edge_fingerprints": new_edges,
            }
        )

    result = {
        "schema": SCHEMA,
        "operation": "explore",
        "mode": mode,
        "seed": seed,
        "execution": SIMULATED,
        "evidence_class": SIMULATED,
        "spec": normalized_spec,
        "cases": cases,
        "episodes": unique_episodes,
        "unique_state_fingerprints": sorted(state_seen),
        "unique_edge_fingerprints": sorted(edge_seen),
        "deduplication": {
            "input_cases": len(cases),
            "unique_cases": len(unique_episodes),
            "duplicate_cases": duplicate_cases,
            "state_count": len(state_seen),
            "edge_count": len(edge_seen),
        },
        "proof": {"simulated": True, "real": False, "needs_dynamic_evidence": True},
    }
    result["exploration_id"] = canonical_digest(result)[:24]
    result["digest"] = canonical_digest(result)
    return result


def explore_edge(spec: Any = None, *, seed: int = 0, budget: int = 16, max_frames: int = 60) -> dict[str, Any]:
    return explore(spec, mode="edge", seed=seed, budget=budget, max_frames=max_frames)


def replay(spec: Any = None, actions: Any = None, *, seed: int = 0, max_frames: int = 600) -> dict[str, Any]:
    return explore(spec, mode="replay", seed=seed, actions=actions, budget=1, max_frames=max_frames)


def inspect_episode(episode: Mapping[str, Any], *, view: str = "summary") -> dict[str, Any]:
    """Return only bounded, in-memory episode views."""

    if not isinstance(episode, Mapping):
        raise ExplorationError("inspect input must be an episode object")
    view = _text(view, "view", maximum=24).lower()
    if view not in {"summary", "timeline", "transitions", "state", "evidence"}:
        raise ExplorationError("view is outside the inspect allowlist")
    if view == "summary":
        fields = (
            "schema",
            "operation",
            "episode_id",
            "execution",
            "evidence_class",
            "identity",
            "seed",
            "frames",
            "status",
            "state_digest",
        )
        return {key: copy.deepcopy(episode.get(key)) for key in fields if key in episode}
    if view == "timeline":
        return {"episode_id": episode.get("episode_id"), "timeline": copy.deepcopy(episode.get("timeline", []))}
    if view == "transitions":
        return {"episode_id": episode.get("episode_id"), "transitions": copy.deepcopy(episode.get("transitions", []))}
    if view == "state":
        return {"episode_id": episode.get("episode_id"), "final_state": copy.deepcopy(episode.get("final_state"))}
    return {
        "episode_id": episode.get("episode_id"),
        "execution": episode.get("execution"),
        "evidence_class": episode.get("evidence_class"),
        "backend": copy.deepcopy(episode.get("backend")),
        "proof": copy.deepcopy(episode.get("proof")),
        "identity": copy.deepcopy(episode.get("identity")),
    }


__all__ = [
    "ExplorationError",
    "MAX_ACTIONS",
    "MAX_FRAMES",
    "SAFE_CONTROLS",
    "SCHEMA",
    "branch_episode",
    "canonical_digest",
    "compare_episodes",
    "explore",
    "explore_edge",
    "inspect_episode",
    "minimize_episode",
    "normalize_actions",
    "normalize_spec",
    "replay",
    "run_episode",
]
