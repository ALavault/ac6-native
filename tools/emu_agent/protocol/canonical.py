"""Canonical JSON and content-addressing helpers for emu-agent.

The wire format is ordinary UTF-8 JSON.  Hashes are computed over a compact,
sorted-key representation with no insignificant whitespace.  This is a small
deterministic subset of JSON canonicalisation suitable for the protocol; it
rejects values JSON implementations commonly encode inconsistently (NaN,
infinity, non-string object keys, and unpaired UTF-16 surrogates).
"""

from __future__ import annotations

import hashlib
import json
import math
from typing import Any, Mapping, Sequence

from .errors import ValidationError


MAX_CANONICAL_DEPTH = 64
MAX_CANONICAL_BYTES = 16 * 1024 * 1024


def _path_item(path: str, item: object) -> str:
    if isinstance(item, int):
        return f"{path}[{item}]"
    return f"{path}.{item}"


def normalize_json(value: Any, *, path: str = "$", _depth: int = 0) -> Any:
    """Return a JSON-only value with deterministic recursive normalisation.

    Dict keys are sorted for the returned copy.  Lists retain order because
    event order is part of an input timeline.  No coercion is performed: a
    malformed number or key is rejected rather than silently changed.
    """

    if _depth > MAX_CANONICAL_DEPTH:
        raise ValidationError("maximum JSON nesting depth exceeded", path=path)
    if value is None or isinstance(value, bool):
        return value
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValidationError("non-finite numbers are not supported", path=path)
        # JSON has one representation for zero in this protocol.  Avoid
        # negative zero differing between otherwise equivalent producers.
        return 0.0 if value == 0.0 else value
    if isinstance(value, str):
        try:
            value.encode("utf-8", "strict")
        except UnicodeEncodeError as error:
            raise ValidationError("string contains an unpaired surrogate", path=path) from error
        return value
    if isinstance(value, Mapping):
        normalised: dict[str, Any] = {}
        for key in sorted(value.keys(), key=lambda candidate: str(candidate)):
            if not isinstance(key, str):
                raise ValidationError("object keys must be strings", path=path)
            try:
                key.encode("utf-8", "strict")
            except UnicodeEncodeError as error:
                raise ValidationError("object key contains an unpaired surrogate", path=path) from error
            if key in normalised:
                raise ValidationError("duplicate object key", path=_path_item(path, key))
            normalised[key] = normalize_json(
                value[key], path=_path_item(path, key), _depth=_depth + 1
            )
        return normalised
    if isinstance(value, Sequence) and not isinstance(value, (bytes, bytearray)):
        return [
            normalize_json(item, path=_path_item(path, index), _depth=_depth + 1)
            for index, item in enumerate(value)
        ]
    raise ValidationError(f"unsupported JSON value type {type(value).__name__}", path=path)


def canonical_json(value: Any) -> str:
    """Encode *value* in the protocol's canonical compact JSON form."""

    normalised = normalize_json(value)
    try:
        encoded = json.dumps(
            normalised,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            separators=(",", ":"),
        )
    except (TypeError, ValueError, UnicodeEncodeError) as error:
        raise ValidationError(f"cannot encode canonical JSON: {error}") from error
    try:
        encoded.encode("utf-8", "strict")
    except UnicodeEncodeError as error:
        raise ValidationError("canonical JSON is not valid UTF-8") from error
    return encoded


def canonical_bytes(value: Any) -> bytes:
    """Return canonical JSON as UTF-8 bytes, bounded before hashing."""

    data = canonical_json(value).encode("utf-8")
    if len(data) > MAX_CANONICAL_BYTES:
        raise ValidationError("canonical JSON exceeds maximum size")
    return data


def sha256_bytes(value: bytes) -> str:
    """Return a lower-case SHA-256 digest for bytes."""

    return hashlib.sha256(value).hexdigest()


def sha256_json(value: Any) -> str:
    """Hash a JSON value after canonical normalisation."""

    return sha256_bytes(canonical_bytes(value))


# Names used by callers that prefer the explicit digest terminology.
canonical_sha256 = sha256_json
hash_json = sha256_json


def canonicalize(value: Any) -> Any:
    """Public spelling for canonical value normalisation."""

    return normalize_json(value)
