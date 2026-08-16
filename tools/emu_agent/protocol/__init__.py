"""Stable public API for the emu-agent/v1 JSON protocol."""

from __future__ import annotations

from .canonical import (
    canonical_bytes,
    canonicalize,
    canonical_json,
    canonical_sha256,
    hash_json,
    normalize_json,
    sha256_bytes,
    sha256_json,
)
from .errors import ProtocolError, UnsupportedVersionError, ValidationError
from .schema import (
    BACKEND_RE,
    DEFAULT_BACKEND,
    DEFAULT_SEED,
    REQUEST_SCHEMA,
    RESULT_SCHEMA,
    hash_inputs,
    input_hash,
    hash_timeline,
    timeline_hash,
    execution_projection,
    adapt_request,
    adapt_result,
    is_valid,
    load_json,
    load_request,
    load_result,
    normalize_request,
    normalize_result,
    normalize_timeline,
    normalize,
    request_hash,
    result_hash,
    validate,
    validate_request,
    validate_result,
)
from .spec import PROTOCOL_SPEC, REQUEST_SPEC, RESULT_SPEC, load_spec
from .v2 import (
    ACTION_SCHEMA, OBSERVATION_SCHEMA, RECEIPT_SCHEMA, V2_SCHEMA,
    unavailable_observation, validate_action, validate_observation, validate_receipt,
)


def run(request, backend=None, **kwargs):
    """Run a local deterministic backend (lazy import avoids a cycle)."""

    from tools.emu_agent.runner import run_local

    return run_local(request, backend=backend, **kwargs)


__all__ = [
    "BACKEND_RE",
    "DEFAULT_BACKEND",
    "DEFAULT_SEED",
    "PROTOCOL_SPEC",
    "ProtocolError",
    "REQUEST_SCHEMA",
    "REQUEST_SPEC",
    "RESULT_SCHEMA",
    "RESULT_SPEC",
    "UnsupportedVersionError",
    "ValidationError",
    "canonical_bytes",
    "canonicalize",
    "canonical_json",
    "canonical_sha256",
    "adapt_request",
    "adapt_result",
    "hash_inputs",
    "input_hash",
    "hash_json",
    "hash_timeline",
    "timeline_hash",
    "execution_projection",
    "is_valid",
    "load_json",
    "load_request",
    "load_result",
    "load_spec",
    "normalize_json",
    "normalize",
    "normalize_request",
    "normalize_result",
    "normalize_timeline",
    "request_hash",
    "result_hash",
    "run",
    "sha256_bytes",
    "sha256_json",
    "validate",
    "validate_request",
    "validate_result",
    "ACTION_SCHEMA",
    "OBSERVATION_SCHEMA",
    "RECEIPT_SCHEMA",
    "V2_SCHEMA",
    "validate_action",
    "validate_observation",
    "validate_receipt",
    "unavailable_observation",
]
