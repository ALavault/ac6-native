"""Machine-readable descriptions of the emu-agent v1 envelopes."""

from __future__ import annotations

import copy
from pathlib import Path
from typing import Any

from .schema import REQUEST_SCHEMA, RESULT_SCHEMA


# These descriptions are intentionally kept alongside the validator.  They
# are useful to a CLI/help surface, while validation remains authoritative and
# does not require a JSON-Schema dependency.
REQUEST_SPEC: dict[str, Any] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": REQUEST_SCHEMA,
    "title": "emu-agent request",
    "type": "object",
    "additionalProperties": False,
    "required": [
        "schema",
        "request_id",
        "target",
        "start",
        "reproducibility",
        "strategy",
        "stop",
        "observe",
        "safety",
        "timeline",
    ],
    "properties": {
        "schema": {"const": REQUEST_SCHEMA},
        "request_id": {"type": "string", "minLength": 1, "maxLength": 128},
        "run_id": {"type": "string", "minLength": 1, "maxLength": 128},
        "target": {"type": "object"},
        "start": {"type": "object"},
        "reproducibility": {"type": "object"},
        "strategy": {"type": "object"},
        "stop": {"type": "object"},
        "observe": {"type": "object"},
        "safety": {"type": "object"},
        "inputs": {"type": ["object", "array", "string", "number", "boolean", "null"]},
        "timeline": {"type": "array"},
        "backend": {"type": "string", "default": "simulated"},
        "seed": {"type": "integer", "minimum": 0, "maximum": 18446744073709551615},
        "options": {"type": "object", "default": {}},
        "_execution": {"type": "object"},
    },
}

RESULT_SPEC: dict[str, Any] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": RESULT_SCHEMA,
    "title": "emu-agent result",
    "type": "object",
    "additionalProperties": False,
    "required": [
        "schema",
        "episode_id",
        "request_id",
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
        "timeline_sha256",
        "request_sha256",
        "output_sha256",
        "semantic_hash",
        "event_sequence_sha256",
        "frames",
        "final_state",
        "error",
    ],
    "properties": {
        "schema": {"const": RESULT_SCHEMA},
        "episode_id": {"type": "string", "minLength": 1, "maxLength": 128},
        "request_id": {"type": "string", "minLength": 1, "maxLength": 128},
        "run_id": {"type": "string", "minLength": 1, "maxLength": 128},
        "target": {"type": "object"},
        "qualified": {"type": "boolean"},
        "qualification_failures": {"type": "array"},
        "stop_reason": {"type": "string", "minLength": 1, "maxLength": 128},
        "guest_progress": {"type": "object"},
        "observer_liveness": {
            "type": "object",
            "required": ["positive_control_expected", "positive_control_seen", "events_before_filter", "events_after_filter"],
            "additionalProperties": False,
        },
        "first_divergence": {"type": ["object", "null"]},
        "artifacts": {"type": ["object", "array"]},
        "status": {"enum": ["ok", "completed", "stopped", "error", "failed"]},
        "backend": {"type": "string"},
        "seed": {"type": "integer", "minimum": 0, "maximum": 18446744073709551615},
        "inputs_sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "input_sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "timeline_sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "request_sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "output_sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "semantic_hash": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "event_sequence_sha256": {"type": "string", "pattern": "^[0-9a-f]{64}$"},
        "frames": {"type": "array"},
        "final_state": {},
        "error": {"type": ["object", "null"]},
    },
}

PROTOCOL_SPEC: dict[str, Any] = {
    "protocol": "emu-agent",
    "version": 1,
    "request": REQUEST_SPEC,
    "result": RESULT_SPEC,
}


def load_spec(source: Any = None, *, kind: str | None = None) -> dict[str, Any]:
    """Return a copy of the protocol description.

    ``source`` is accepted for CLI ergonomics: ``"request"``/``"result"``
    select a built-in section, and a path or JSON text may provide a custom
    spec document.  Custom specs are not trusted as validators; callers still
    pass documents through :func:`tools.emu_agent.protocol.validate`.
    """

    selected: Any
    if source is None:
        selected = PROTOCOL_SPEC
    elif isinstance(source, str) and source in ("request", "input", "result", "output"):
        selected = REQUEST_SPEC if source in ("request", "input") else RESULT_SPEC
    elif isinstance(source, Path):
        import json

        try:
            selected = json.loads(source.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            raise ValueError(f"cannot read protocol spec {source}: {error}") from error
    elif isinstance(source, str):
        import json

        try:
            selected = json.loads(source)
        except json.JSONDecodeError as error:
            path = Path(source)
            try:
                selected = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, UnicodeError, json.JSONDecodeError) as path_error:
                raise ValueError("unknown spec section or invalid JSON spec text") from path_error
    elif isinstance(source, dict):
        selected = source
    else:
        raise TypeError("spec source must be omitted, a section name, path, or mapping")
    if kind is not None:
        if kind in ("request", "input"):
            if selected is PROTOCOL_SPEC:
                selected = selected["request"]
        elif kind in ("result", "output"):
            if selected is PROTOCOL_SPEC:
                selected = selected["result"]
        else:
            raise ValueError(f"unsupported spec kind {kind!r}")
    return copy.deepcopy(selected)
