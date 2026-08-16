"""Versioned emu-agent request/result contracts.

The validator intentionally returns a fresh canonical copy.  A caller must use
that copy for execution and hashing; retaining a caller-owned mutable object
would make a digest stale between validation and execution.
"""

from __future__ import annotations

import copy
import json
import re
from pathlib import Path
from typing import Any, Iterable, Mapping

from .canonical import normalize_json, sha256_json
from .errors import UnsupportedVersionError, ValidationError


REQUEST_SCHEMA = "emu-agent/v1"
RESULT_SCHEMA = "emu-agent-result/v1"
SCHEMA = REQUEST_SCHEMA
RESULT = RESULT_SCHEMA
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
IDENTIFIER_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.:/-]{0,127}$")
BACKEND_RE = re.compile(r"^[a-z][a-z0-9_-]{0,31}$")
STATUS_VALUES = frozenset(("ok", "completed", "stopped", "error", "failed"))
ERROR_STATUS_VALUES = frozenset(("error", "failed"))
DEFAULT_BACKEND = "simulated"
DEFAULT_SEED = 0
MAX_SEED = (1 << 64) - 1
MAX_FRAME = (1 << 53) - 1

_REQUEST_KEYS = frozenset(
    (
        "schema",
        "request_id",
        "run_id",
        "target",
        "start",
        "reproducibility",
        "strategy",
        "stop",
        "observe",
        "safety",
        "inputs",
        "timeline",
        "backend",
        "seed",
        "options",
        "_execution",
    )
)
_RESULT_KEYS = frozenset(
    (
        "schema",
        "episode_id",
        "request_id",
        "run_id",
        "target",
        "qualified",
        "qualification_failures",
        "stop_reason",
        "guest_progress",
        "observer_liveness",
        "first_divergence",
        "artifacts",
        "status",
        "backend",
        "seed",
        "inputs_sha256",
        "input_sha256",
        "timeline_sha256",
        "request_sha256",
        "output_sha256",
        "semantic_hash",
        "event_sequence_sha256",
        "frames",
        "final_state",
        "error",
    )
)
_EVENT_META_KEYS = frozenset(
    ("frame", "type", "kind", "event", "payload", "value", "input", "inputs", "data",
     "at", "controller", "state", "duration")
)


def _require_mapping(value: Any, *, path: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ValidationError("expected a JSON object", path=path)
    return value


def _reject_unknown(value: Mapping[str, Any], allowed: frozenset[str], *, path: str) -> None:
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ValidationError(f"unsupported field(s): {', '.join(unknown)}", path=path)


def _required_string(value: Mapping[str, Any], key: str, *, path: str, pattern: re.Pattern[str] | None = None) -> str:
    candidate = value.get(key)
    if not isinstance(candidate, str) or not candidate:
        raise ValidationError("must be a non-empty string", path=f"{path}.{key}")
    if pattern is not None and pattern.fullmatch(candidate) is None:
        raise ValidationError("contains unsupported characters or is too long", path=f"{path}.{key}")
    return candidate


def _uint(value: Any, *, path: str, maximum: int = MAX_FRAME) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValidationError("must be an unsigned integer", path=path)
    if value < 0 or value > maximum:
        raise ValidationError(f"must be in range 0..{maximum}", path=path)
    return value


def _hash(value: Any, *, path: str) -> str:
    if not isinstance(value, str) or SHA256_RE.fullmatch(value) is None:
        raise ValidationError("must be a lower-case SHA-256 hexadecimal digest", path=path)
    return value


def _normalise_event(value: Any, *, index: int) -> dict[str, Any]:
    path = f"$.timeline[{index}]"
    event = _require_mapping(value, path=path)

    # Public emu-agent/v1 controller records are poll-clocked and deliberately
    # do not expose a host sleep primitive.  Internally they are represented by
    # the same monotonic ``frame`` field used by the generic runner; the full
    # clock/state envelope remains in the payload and therefore in the digest.
    if "at" in event:
        allowed = {"at", "controller", "state", "duration"}
        unknown = sorted(set(event) - allowed)
        if unknown:
            raise ValidationError(
                f"unsupported controller event field(s): {', '.join(unknown)}", path=path
            )
        at = _require_mapping(event["at"], path=f"{path}.at")
        if set(at) != {"clock", "index"}:
            raise ValidationError("at must contain clock and index", path=f"{path}.at")
        clock = at.get("clock")
        if clock not in {"xam_input_poll", "guest_frame", "present"}:
            raise ValidationError("unsupported controller clock", path=f"{path}.at.clock")
        at_index = _uint(at.get("index"), path=f"{path}.at.index")
        controller = event.get("controller", 0)
        if isinstance(controller, bool) or not isinstance(controller, int) or not 0 <= controller <= 3:
            raise ValidationError("controller must be in range 0..3", path=f"{path}.controller")
        state = _require_mapping(event.get("state"), path=f"{path}.state")
        allowed_state = {"buttons", "lx", "ly", "rx", "ry", "lt", "rt"}
        if set(state) != allowed_state:
            raise ValidationError("state must contain exactly buttons/lx/ly/rx/ry/lt/rt", path=f"{path}.state")
        buttons = state["buttons"]
        if not isinstance(buttons, list) or any(not isinstance(button, str) for button in buttons):
            raise ValidationError("state.buttons must be an array of strings", path=f"{path}.state.buttons")
        bounded = {"lx": (-32768, 32767), "ly": (-32768, 32767), "rx": (-32768, 32767), "ry": (-32768, 32767), "lt": (0, 255), "rt": (0, 255)}
        canonical_state: dict[str, Any] = {"buttons": sorted(set(buttons))}
        for key, (minimum, maximum) in bounded.items():
            raw = state[key]
            if isinstance(raw, bool) or not isinstance(raw, int) or raw < minimum or raw > maximum:
                raise ValidationError(f"{key} is outside its controller range", path=f"{path}.state.{key}")
            canonical_state[key] = raw
        duration = _require_mapping(event.get("duration"), path=f"{path}.duration")
        if set(duration) != {"clock", "count"} or duration.get("clock") != "xam_input_poll":
            raise ValidationError("duration must be xam_input_poll/count", path=f"{path}.duration")
        count = duration.get("count")
        if isinstance(count, bool) or not isinstance(count, int) or count <= 0 or count > MAX_FRAME:
            raise ValidationError("duration.count must be a positive bounded integer", path=f"{path}.duration.count")
        canonical_at = {"clock": clock, "index": at_index}
        canonical_duration = {"clock": "xam_input_poll", "count": count}
        return {
            "frame": at_index,
            "type": "controller",
            "payload": {
                "at": canonical_at,
                "controller": controller,
                "state": canonical_state,
                "duration": canonical_duration,
            },
        }
    if "frame" not in event:
        raise ValidationError("event frame is required", path=path)
    frame = _uint(event["frame"], path=f"{path}.frame")
    unknown = sorted(set(event) - _EVENT_META_KEYS)

    # A timeline event is intentionally permissive about the payload's shape,
    # but not about its control envelope.  This permits future input controls
    # without making the v1 validator guess their semantics.
    event_type: Any = event.get("type", event.get("kind", event.get("event", "input")))
    if not isinstance(event_type, str) or not event_type or len(event_type) > 64:
        raise ValidationError("event type must be a non-empty string", path=f"{path}.type")
    if any(key in event for key in ("type", "kind", "event")):
        for key in ("type", "kind", "event"):
            if key in event and event[key] != event_type:
                raise ValidationError("event type aliases disagree", path=path)

    payload_keys = [key for key in ("payload", "value", "input", "inputs", "data") if key in event]
    if len(payload_keys) > 1:
        raise ValidationError("event payload aliases are ambiguous", path=path)
    if payload_keys:
        payload = normalize_json(event[payload_keys[0]], path=f"{path}.{payload_keys[0]}")
        if unknown:
            raise ValidationError(
                f"unsupported event field(s): {', '.join(unknown)}", path=path
            )
    else:
        # Compact event records such as {"frame": 4, "button": "A"} are
        # accepted as input payloads while retaining strict top-level fields.
        payload = normalize_json(
            {key: event[key] for key in unknown}, path=f"{path}.payload"
        )
    return {"frame": frame, "type": event_type, "payload": payload}


def normalize_timeline(value: Any, *, require_monotonic: bool = True) -> list[dict[str, Any]]:
    """Canonicalise a timeline and reject ambiguous frame ordering."""

    if not isinstance(value, list):
        raise ValidationError("must be a JSON array", path="$.timeline")
    timeline = [_normalise_event(item, index=index) for index, item in enumerate(value)]
    if require_monotonic:
        previous = -1
        for index, event in enumerate(timeline):
            frame = event["frame"]
            if frame < previous:
                raise ValidationError(
                    "timeline frames must be non-decreasing", path=f"$.timeline[{index}].frame"
                )
            previous = frame
    return timeline


def hash_inputs(inputs: Any) -> str:
    """Hash canonical request inputs (without timeline or execution metadata)."""

    return sha256_json(normalize_json(inputs, path="$.inputs"))


def hash_timeline(timeline: Any) -> str:
    """Hash canonical, ordered timeline events."""

    return sha256_json(normalize_timeline(timeline))


input_hash = hash_inputs
timeline_hash = hash_timeline


def _normalise_request(value: Any) -> dict[str, Any]:
    request = _require_mapping(value, path="$")
    _reject_unknown(request, _REQUEST_KEYS, path="$")
    if request.get("schema") != REQUEST_SCHEMA:
        declared = request.get("schema")
        if isinstance(declared, str) and declared.startswith("emu-agent/"):
            raise UnsupportedVersionError(
                f"unsupported request schema {declared!r}", path="$.schema"
            )
        raise ValidationError(f"schema must be {REQUEST_SCHEMA!r}", path="$.schema")

    request_id = request.get("request_id", request.get("run_id"))
    if "request_id" in request and "run_id" in request and request["request_id"] != request["run_id"]:
        raise ValidationError("request_id and run_id aliases disagree", path="$")
    if not isinstance(request_id, str) or IDENTIFIER_RE.fullmatch(request_id) is None:
        raise ValidationError("must be a non-empty bounded identifier", path="$.request_id")
    # These seven fields are the public envelope.  They remain structured
    # objects rather than being interpreted by this transport layer: a future
    # qualified backend owns their domain semantics.  Requiring objects here
    # prevents accidental strings/paths/commands from crossing the boundary.
    full_fields: dict[str, Any] = {}
    for name in (
        "target",
        "start",
        "reproducibility",
        "strategy",
        "stop",
        "observe",
        "safety",
    ):
        if name not in request:
            raise ValidationError("required field is missing", path=f"$.{name}")
        candidate = request[name]
        if not isinstance(candidate, Mapping):
            raise ValidationError("must be a JSON object", path=f"$.{name}")
        full_fields[name] = normalize_json(candidate, path=f"$.{name}")
    # The published contract carries controller state in ``timeline``.  Keep
    # the older generic ``inputs`` member optional and derive an empty object
    # when absent so v1 clients need not duplicate their timeline.
    if "timeline" not in request:
        raise ValidationError("required field is missing", path="$.timeline")
    inputs = normalize_json(request.get("inputs", {}), path="$.inputs")
    timeline = normalize_timeline(request["timeline"])
    backend = request.get("backend", DEFAULT_BACKEND)
    if not isinstance(backend, str) or BACKEND_RE.fullmatch(backend) is None:
        raise ValidationError("must be a bounded backend identifier", path="$.backend")
    seed = request.get("seed", DEFAULT_SEED)
    seed = _uint(seed, path="$.seed", maximum=MAX_SEED)
    options = request.get("options", {})
    if not isinstance(options, Mapping):
        raise ValidationError("must be a JSON object", path="$.options")
    options = normalize_json(options, path="$.options")
    execution = request.get("_execution")
    if execution is not None:
        if not isinstance(execution, Mapping):
            raise ValidationError("must be a JSON object", path="$._execution")
        execution = normalize_json(execution, path="$._execution")
    result = {
        "schema": REQUEST_SCHEMA,
        "request_id": request_id,
        **full_fields,
        "inputs": inputs,
        "timeline": timeline,
        "backend": backend,
        "seed": seed,
        "options": options,
    }
    if execution is not None:
        result["_execution"] = execution
    return result


def normalize_request(value: Any) -> dict[str, Any]:
    """Return the canonical request envelope, raising on any invalid field."""

    return _normalise_request(value)


def validate_request(value: Any) -> dict[str, Any]:
    """Validate a request and return its immutable-by-convention copy."""

    return _normalise_request(value)


def _normalise_frame(value: Any, *, index: int) -> dict[str, Any]:
    path = f"$.frames[{index}]"
    frame = _require_mapping(value, path=path)
    allowed = frozenset(("frame", "event_index", "state", "state_sha256"))
    _reject_unknown(frame, allowed, path=path)
    if set(frame) != allowed:
        missing = sorted(allowed - set(frame))
        raise ValidationError(f"missing field(s): {', '.join(missing)}", path=path)
    result = {
        "frame": _uint(frame["frame"], path=f"{path}.frame"),
        "event_index": _uint(frame["event_index"], path=f"{path}.event_index"),
        "state": normalize_json(frame["state"], path=f"{path}.state"),
        "state_sha256": _hash(frame["state_sha256"], path=f"{path}.state_sha256"),
    }
    expected = sha256_json(result["state"])
    if result["state_sha256"] != expected:
        raise ValidationError("does not match canonical state hash", path=f"{path}.state_sha256")
    return result


def _normalise_result(value: Any) -> dict[str, Any]:
    result = _require_mapping(value, path="$")
    _reject_unknown(result, _RESULT_KEYS, path="$")
    if result.get("schema") != RESULT_SCHEMA:
        declared = result.get("schema")
        if isinstance(declared, str) and declared.startswith("emu-agent-result/"):
            raise UnsupportedVersionError(
                f"unsupported result schema {declared!r}", path="$.schema"
            )
        raise ValidationError(f"schema must be {RESULT_SCHEMA!r}", path="$.schema")
    episode_id = result.get("episode_id", result.get("request_id", result.get("run_id")))
    request_id = result.get("request_id", result.get("run_id", episode_id))
    if "episode_id" in result and result["episode_id"] != request_id:
        raise ValidationError("episode_id and request_id aliases disagree", path="$")
    if "request_id" in result and "run_id" in result and result["request_id"] != result["run_id"]:
        raise ValidationError("request_id and run_id aliases disagree", path="$")
    if not isinstance(episode_id, str) or IDENTIFIER_RE.fullmatch(episode_id) is None:
        raise ValidationError("must be a non-empty bounded identifier", path="$.episode_id")
    if not isinstance(request_id, str) or IDENTIFIER_RE.fullmatch(request_id) is None:
        raise ValidationError("must be a non-empty bounded identifier", path="$.request_id")
    if "target" not in result:
        raise ValidationError("required field is missing", path="$.target")
    target = result["target"]
    if not isinstance(target, Mapping):
        raise ValidationError("must be a JSON object", path="$.target")
    target = normalize_json(target, path="$.target")
    qualified = result.get("qualified")
    if not isinstance(qualified, bool):
        raise ValidationError("must be a boolean", path="$.qualified")
    failures = result.get("qualification_failures")
    if not isinstance(failures, list):
        raise ValidationError("must be a JSON array", path="$.qualification_failures")
    failures = normalize_json(failures, path="$.qualification_failures")
    for index, failure in enumerate(failures):
        if not isinstance(failure, (str, Mapping)):
            raise ValidationError(
                "each qualification failure must be a string or object",
                path=f"$.qualification_failures[{index}]",
            )
    if qualified and failures:
        raise ValidationError(
            "qualified result cannot contain qualification failures",
            path="$.qualification_failures",
        )
    stop_reason = result.get("stop_reason")
    if not isinstance(stop_reason, str) or not stop_reason or len(stop_reason) > 128:
        raise ValidationError("must be a bounded non-empty string", path="$.stop_reason")
    guest_progress = result.get("guest_progress")
    if not isinstance(guest_progress, Mapping):
        raise ValidationError("must be a JSON object", path="$.guest_progress")
    guest_progress = normalize_json(guest_progress, path="$.guest_progress")
    observer_liveness = result.get("observer_liveness")
    if isinstance(observer_liveness, bool):
        # Accept the first draft's scalar form on input, but publish the v1
        # structured form below so positive-control accounting is never lost.
        observer_liveness = {
            "positive_control_expected": observer_liveness,
            "positive_control_seen": observer_liveness,
            "events_before_filter": 0,
            "events_after_filter": 0,
        }
    elif isinstance(observer_liveness, Mapping):
        observer_liveness = normalize_json(observer_liveness, path="$.observer_liveness")
        required_liveness = frozenset(
            ("positive_control_expected", "positive_control_seen", "events_before_filter", "events_after_filter")
        )
        if set(observer_liveness) != required_liveness:
            missing = sorted(required_liveness - set(observer_liveness))
            extra = sorted(set(observer_liveness) - required_liveness)
            details = []
            if missing:
                details.append("missing " + ", ".join(missing))
            if extra:
                details.append("unsupported " + ", ".join(extra))
            raise ValidationError("; ".join(details) or "invalid liveness object", path="$.observer_liveness")
        for key in ("positive_control_expected", "positive_control_seen"):
            if not isinstance(observer_liveness[key], bool):
                raise ValidationError("must be a boolean", path=f"$.observer_liveness.{key}")
        for key in ("events_before_filter", "events_after_filter"):
            _uint(observer_liveness[key], path=f"$.observer_liveness.{key}")
    else:
        raise ValidationError("must be a boolean or liveness object", path="$.observer_liveness")
    first_divergence = normalize_json(result.get("first_divergence"), path="$.first_divergence")
    if first_divergence is not None and not isinstance(first_divergence, Mapping):
        raise ValidationError("must be null or a JSON object", path="$.first_divergence")
    artifacts = result.get("artifacts")
    if not isinstance(artifacts, (Mapping, list)):
        raise ValidationError("must be a JSON object or array", path="$.artifacts")
    artifacts = normalize_json(artifacts, path="$.artifacts")
    status = result.get("status")
    if status not in STATUS_VALUES:
        raise ValidationError("must be either 'ok' or 'error'", path="$.status")
    backend = result.get("backend")
    if not isinstance(backend, str) or BACKEND_RE.fullmatch(backend) is None:
        raise ValidationError("must be a bounded backend identifier", path="$.backend")
    seed = _uint(result.get("seed"), path="$.seed", maximum=MAX_SEED)
    inputs_sha256 = _hash(
        result.get("inputs_sha256", result.get("input_sha256")), path="$.inputs_sha256"
    )
    if "inputs_sha256" in result and "input_sha256" in result and result["inputs_sha256"] != result["input_sha256"]:
        raise ValidationError("inputs_sha256 and input_sha256 aliases disagree", path="$")
    timeline_sha256 = _hash(result.get("timeline_sha256"), path="$.timeline_sha256")
    request_sha256 = _hash(result.get("request_sha256"), path="$.request_sha256")
    output_sha256 = _hash(result.get("output_sha256"), path="$.output_sha256")
    semantic_hash = _hash(result.get("semantic_hash"), path="$.semantic_hash")
    event_sequence_sha256 = _hash(result.get("event_sequence_sha256"), path="$.event_sequence_sha256")
    frames_value = result.get("frames")
    if not isinstance(frames_value, list):
        raise ValidationError("must be a JSON array", path="$.frames")
    frames = [_normalise_frame(item, index=index) for index, item in enumerate(frames_value)]
    previous_frame = -1
    for index, frame in enumerate(frames):
        if frame["frame"] < previous_frame:
            raise ValidationError("result frames must be non-decreasing", path=f"$.frames[{index}].frame")
        previous_frame = frame["frame"]
    expected_output = sha256_json(frames)
    if output_sha256 != expected_output:
        raise ValidationError("does not match canonical frame hash", path="$.output_sha256")
    final_state = normalize_json(result.get("final_state"), path="$.final_state")
    if status not in ERROR_STATUS_VALUES:
        if result.get("error") is not None:
            raise ValidationError("successful result must have a null error", path="$.error")
    else:
        error = result.get("error")
        if not isinstance(error, Mapping):
            raise ValidationError("error result must include an error object", path="$.error")
        error = normalize_json(error, path="$.error")
        if set(error) != {"code", "message"}:
            raise ValidationError("error must contain exactly code and message", path="$.error")
        if not isinstance(error["code"], str) or not error["code"]:
            raise ValidationError("error code must be a non-empty string", path="$.error.code")
        if not isinstance(error["message"], str) or not error["message"]:
            raise ValidationError("error message must be a non-empty string", path="$.error.message")
    return {
        "schema": RESULT_SCHEMA,
        "episode_id": episode_id,
        "request_id": request_id,
        "target": target,
        "qualified": qualified,
        "qualification_failures": failures,
        "stop_reason": stop_reason,
        "guest_progress": guest_progress,
        "observer_liveness": observer_liveness,
        "first_divergence": first_divergence,
        "artifacts": artifacts,
        "status": status,
        "backend": backend,
        "seed": seed,
        "inputs_sha256": inputs_sha256,
        "timeline_sha256": timeline_sha256,
        "request_sha256": request_sha256,
        "output_sha256": output_sha256,
        "semantic_hash": semantic_hash,
        "event_sequence_sha256": event_sequence_sha256,
        "frames": frames,
        "final_state": final_state,
        "error": None if status not in ERROR_STATUS_VALUES else error,
    }


def normalize_result(value: Any) -> dict[str, Any]:
    """Return the canonical result envelope, raising on any invalid field."""

    return _normalise_result(value)


def validate_result(value: Any) -> dict[str, Any]:
    """Validate a result and return its canonical copy."""

    return _normalise_result(value)


def load_request(source: Any) -> dict[str, Any]:
    """Decode and validate one request source in a single fail-closed call."""

    return validate_request(load_json(source))


def load_result(source: Any) -> dict[str, Any]:
    """Decode and validate one result source in a single fail-closed call."""

    return validate_result(load_json(source))


def validate(value: Any, *, kind: str | None = None) -> dict[str, Any]:
    """Validate either protocol document, selected by schema unless explicit."""

    if kind is not None:
        if kind in ("request", "input", "emu-agent/v1"):
            return validate_request(value)
        if kind in ("result", "output", "emu-agent-result/v1"):
            return validate_result(value)
        raise ValidationError(f"unsupported validation kind {kind!r}", path="$.kind")
    if isinstance(value, Mapping):
        schema = value.get("schema")
        if schema == REQUEST_SCHEMA:
            return validate_request(value)
        if schema == RESULT_SCHEMA:
            return validate_result(value)
    raise ValidationError("schema does not identify a supported emu-agent document", path="$.schema")


def is_valid(value: Any, *, kind: str | None = None) -> bool:
    """Boolean convenience wrapper that never leaks malformed input onward."""

    try:
        validate(value, kind=kind)
    except (ProtocolError, TypeError, ValueError):
        return False
    return True


def request_hash(request: Any) -> str:
    """Hash the canonical request envelope used by a runner result."""

    return sha256_json(validate_request(request))


def result_hash(result: Any) -> str:
    """Hash a canonical result envelope (including its self-reported hashes)."""

    return sha256_json(validate_result(result))


def execution_projection(request: Any) -> dict[str, Any]:
    """Project the full public envelope to the backend execution view.

    The projection is deliberately explicit.  It keeps target and safety
    policy attached to a run while preventing a backend from accidentally
    depending on protocol aliases or host-only wrapper fields.
    """

    canonical = validate_request(request)
    return {
        "target": canonical["target"],
        "start": canonical["start"],
        "reproducibility": canonical["reproducibility"],
        "strategy": canonical["strategy"],
        "stop": canonical["stop"],
        "observe": canonical["observe"],
        "safety": canonical["safety"],
        "inputs": canonical["inputs"],
        "timeline": canonical["timeline"],
        "backend": canonical["backend"],
        "seed": canonical["seed"],
        "options": canonical["options"],
    }


def normalize(value: Any, *, kind: str | None = None) -> dict[str, Any]:
    """Short alias for :func:`validate` used by small protocol clients."""

    return validate(value, kind=kind)


def adapt_request(value: Any) -> dict[str, Any]:
    """Attach a deterministic private execution projection to a public request.

    The public request remains unchanged when passed to
    :func:`normalize_request`; callers that hand the document to a backend can
    use this adapter to retain the full envelope while making the internal
    view explicit under the reserved ``_execution`` key.
    """

    canonical = normalize_request(value)
    canonical["_execution"] = execution_projection(canonical)
    return canonical


def adapt_result(value: Any) -> dict[str, Any]:
    """Validate a backend result before exposing it on the public wire."""

    return normalize_result(value)


def _object_pairs_no_duplicates(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    output: dict[str, Any] = {}
    for key, value in pairs:
        if key in output:
            raise ValidationError(f"duplicate JSON object key {key!r}")
        output[key] = value
    return output


def load_json(source: Any) -> Any:
    """Load a JSON document from a mapping, path, bytes, or text.

    Duplicate object keys are rejected at parse time, before protocol
    validation can observe an already-overwritten value.
    """

    if isinstance(source, Mapping) or isinstance(source, list):
        return copy.deepcopy(source)
    if isinstance(source, Path):
        try:
            raw = source.read_bytes()
        except OSError as error:
            raise ValidationError(f"cannot read JSON document: {error}") from error
    elif isinstance(source, bytes):
        raw = source
    elif isinstance(source, str):
        raw = source.encode("utf-8")
    else:
        raise ValidationError("expected a mapping, JSON text, bytes, or path")
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=_object_pairs_no_duplicates)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValidationError(f"invalid UTF-8 JSON: {error}") from error
