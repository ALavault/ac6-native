"""Deterministic local runner for emu-agent/v1 requests."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any, Mapping

from tools.emu_agent.protocol import (
    RESULT_SCHEMA,
    hash_inputs,
    hash_timeline,
    load_request,
    normalize_json,
    request_hash,
    sha256_json,
    validate_result,
)
from tools.emu_agent.protocol.errors import ProtocolError, ValidationError

from .backend import Backend, SimulatedBackend


BackendFactory = Callable[[], Backend]
BackendLike = Backend | str | None


def _backend_name(backend: object) -> str:
    name = getattr(backend, "name", None)
    if not isinstance(name, str) or not name:
        raise ProtocolError("backend object must expose a non-empty name")
    return name


def _resolve_backend(request: Mapping[str, Any], backend: BackendLike) -> Backend:
    requested_name = request["backend"]
    if backend is None:
        if requested_name == "simulated":
            resolved: object = SimulatedBackend()
        elif requested_name == "xenia":
            from tools.emu_agent.backends.xenia import XeniaBackend

            resolved = XeniaBackend()
        elif requested_name == "pcsx2":
            # The adapter is intentionally fail-closed; it supplies the
            # structured transport error rather than silently treating a
            # requested PCSX2 episode as a simulator run.
            from tools.emu_agent.backends.pcsx2 import Pcsx2Backend

            resolved = Pcsx2Backend.fixture()
        else:
            raise ProtocolError(f"no local backend is registered for {requested_name!r}")
    elif isinstance(backend, str):
        if backend != requested_name:
            raise ProtocolError(
                f"requested backend {requested_name!r} does not match override {backend!r}"
            )
        if backend == "simulated":
            resolved = SimulatedBackend()
        elif backend == "xenia":
            from tools.emu_agent.backends.xenia import XeniaBackend

            resolved = XeniaBackend()
        elif backend == "pcsx2":
            from tools.emu_agent.backends.pcsx2 import Pcsx2Backend

            resolved = Pcsx2Backend.fixture()
        else:
            raise ProtocolError(f"no local backend is registered for {backend!r}")
    else:
        resolved = backend
    if not isinstance(resolved, Backend):
        # A structural adapter keeps the seam usable for tiny test backends,
        # while still validating every returned value at the runner boundary.
        for method in ("start", "step", "finish"):
            if not callable(getattr(resolved, method, None)):
                raise ProtocolError(f"backend object is missing {method}()")
    if _backend_name(resolved) != requested_name:
        raise ProtocolError(
            f"backend object { _backend_name(resolved)!r} does not match request {requested_name!r}"
        )
    return resolved  # type: ignore[return-value]


def _enforce_safety(request: Mapping[str, Any]) -> None:
    """Reject host/guest mutation before a backend is started.

    The protocol keeps the safety object extensible for independent contract
    tests, but the shipped runner has a deliberately smaller authority: no
    guest writes, synthetic state, or shell access.  A future explicit
    ``edge_synthetic`` backend must provide its own separately reviewed runner
    rather than weakening this default path.
    """

    safety = request.get("safety", {})
    if not isinstance(safety, Mapping):
        raise ValidationError("safety must be an object", path="$.safety")
    for key in ("allow_guest_write", "allow_synthetic_state", "allow_shell"):
        if key in safety and safety[key] is not False:
            raise ProtocolError(f"safety.{key} must be false for the local runner")


def _event_stop_reason(request: Mapping[str, Any], event: Mapping[str, Any]) -> str | None:
    """Apply the small, deterministic subset of stop predicates the model can see."""

    stop = request.get("stop", {})
    if not isinstance(stop, Mapping):
        return None
    predicates = stop.get("any", [])
    if not isinstance(predicates, list):
        return None
    event_type = event.get("type")
    payload = event.get("payload")
    for predicate in predicates:
        if not isinstance(predicate, Mapping):
            continue
        expected = predicate.get("event")
        if isinstance(expected, str) and (event_type == expected or (isinstance(payload, Mapping) and payload.get("event") == expected)):
            return "event:" + expected
    return None


class LocalRunner:
    """Execute one validated request against one deterministic backend."""

    def __init__(self, backend: BackendLike = None) -> None:
        self._backend = backend

    def run(self, request: Any) -> dict[str, Any]:
        canonical_request = load_request(request)
        _enforce_safety(canonical_request)
        inputs_hash_before = hash_inputs(canonical_request["inputs"])
        timeline_hash_before = hash_timeline(canonical_request["timeline"])
        backend = _resolve_backend(canonical_request, self._backend)
        backend.start(canonical_request)
        frames: list[dict[str, Any]] = []
        stop_reason = None
        for event_index, event in enumerate(canonical_request["timeline"]):
            state = backend.step(event)
            state = normalize_json(state, path=f"$.frames[{event_index}].state")
            if not isinstance(state, Mapping):
                raise ValidationError(
                    "backend step must return a JSON object",
                    path=f"$.frames[{event_index}].state",
                )
            state = dict(state)
            frames.append(
                {
                    "frame": event["frame"],
                    "event_index": event_index,
                    "state": state,
                    "state_sha256": sha256_json(state),
                }
            )
            stop_reason = _event_stop_reason(canonical_request, event)
            if stop_reason is not None:
                break
        final_state = normalize_json(backend.finish(), path="$.final_state")
        if not isinstance(final_state, Mapping):
            raise ValidationError("backend finish must return a JSON object", path="$.final_state")
        # Emulator adapters may provide result-level qualification metadata
        # without contaminating the public final-state snapshot.  The marker
        # is private to the runner and is stripped before validation.
        metadata = final_state.get("_result_metadata", {})
        if metadata is None:
            metadata = {}
        if not isinstance(metadata, Mapping):
            raise ValidationError("backend result metadata must be an object", path="$.final_state._result_metadata")
        final_state = dict(final_state)
        final_state.pop("_result_metadata", None)
        if hash_inputs(canonical_request["inputs"]) != inputs_hash_before:
            raise ProtocolError("backend mutated the immutable input source")
        if hash_timeline(canonical_request["timeline"]) != timeline_hash_before:
            raise ProtocolError("backend mutated the immutable input timeline")
        result = {
            "schema": RESULT_SCHEMA,
            "episode_id": canonical_request["request_id"],
            "request_id": canonical_request["request_id"],
            "target": canonical_request["target"],
            "qualified": bool(metadata.get("qualified", False)),
            "qualification_failures": normalize_json(metadata.get("qualification_failures", ["simulated-backend-not-qualified"] if canonical_request["backend"] == "simulated" else ["backend-not-qualified"]), path="$.qualification_failures"),
            "stop_reason": str(metadata.get("stop_reason", stop_reason or ("timeline-exhausted" if canonical_request["timeline"] else "empty-timeline"))),
            "guest_progress": normalize_json(metadata.get("guest_progress", {
                "events": len(frames),
                "start_poll": 0,
                "end_poll": None,
                "start_frame": 0,
                "end_frame": frames[-1]["frame"] if frames else 0,
                "last_frame": frames[-1]["frame"] if frames else 0,
                "presented": False,
            }), path="$.guest_progress"),
            "observer_liveness": normalize_json(metadata.get("observer_liveness", {
                "positive_control_expected": True,
                "positive_control_seen": False,
                "events_before_filter": len(frames),
                "events_after_filter": 0,
            }), path="$.observer_liveness"),
            "first_divergence": None,
            "artifacts": normalize_json(metadata.get("artifacts", {}), path="$.artifacts"),
            "status": str(metadata.get("status", "completed")),
            "backend": canonical_request["backend"],
            "seed": canonical_request["seed"],
            "inputs_sha256": inputs_hash_before,
            "timeline_sha256": timeline_hash_before,
            "request_sha256": request_hash(canonical_request),
            "output_sha256": sha256_json(frames),
            "semantic_hash": sha256_json({"frames": frames, "final_state": dict(final_state)}),
            "event_sequence_sha256": hash_timeline(canonical_request["timeline"]),
            "frames": frames,
            "final_state": dict(final_state),
            "error": normalize_json(metadata.get("error"), path="$.error"),
        }
        # Validate and return another fresh canonical copy.  This catches a
        # custom backend that returned a subtly non-JSON value or bad digest.
        return validate_result(result)


def run_local(request: Any, *, backend: BackendLike = None) -> dict[str, Any]:
    """Run a request locally; malformed/unsupported inputs raise closed."""

    return LocalRunner(backend=backend).run(request)


run = run_local


def run_safe(request: Any, *, backend: BackendLike = None) -> dict[str, Any]:
    """Return a deterministic error envelope for callers that need one.

    Validation still happens before any backend is started.  The error result
    uses zero hashes only when no valid request exists, so consumers must not
    treat it as an executable or parity-bearing run.
    """

    try:
        return run_local(request, backend=backend)
    except Exception as error:  # deliberate boundary for a CLI/result caller
        try:
            canonical_request = load_request(request)
        except Exception:
            raise
        message = str(error) or error.__class__.__name__
        result = {
            "schema": RESULT_SCHEMA,
            "episode_id": canonical_request["request_id"],
            "request_id": canonical_request["request_id"],
            "target": canonical_request["target"],
            "qualified": False,
            "qualification_failures": ["local-run-error"],
            "stop_reason": "backend-error",
            "guest_progress": {"events": 0, "start_poll": 0, "end_poll": None, "start_frame": 0, "end_frame": 0, "last_frame": 0, "presented": False},
            "observer_liveness": {
                "positive_control_expected": True,
                "positive_control_seen": False,
                "events_before_filter": 0,
                "events_after_filter": 0,
            },
            "first_divergence": None,
            "artifacts": {},
            "status": "error",
            "backend": canonical_request["backend"],
            "seed": canonical_request["seed"],
            "inputs_sha256": hash_inputs(canonical_request["inputs"]),
            "timeline_sha256": hash_timeline(canonical_request["timeline"]),
            "request_sha256": request_hash(canonical_request),
            "output_sha256": sha256_json([]),
            "semantic_hash": sha256_json({"frames": [], "final_state": {}}),
            "event_sequence_sha256": hash_timeline(canonical_request["timeline"]),
            "frames": [],
            "final_state": {},
            "error": {"code": error.__class__.__name__, "message": message},
        }
        return validate_result(result)
