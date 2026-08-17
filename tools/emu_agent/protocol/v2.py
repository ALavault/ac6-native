"""Strict AC6 emu-agent/v2 envelopes.

This module is deliberately transport agnostic.  It validates JSON data only;
it never accepts paths, shell commands, guest addresses, or opaque host state.
"""
from __future__ import annotations

import copy
import hashlib
import json
import math
from typing import Any, Mapping

from .errors import ValidationError

ACTION_SCHEMA = "ac6-agent-action/v1"
OBSERVATION_SCHEMA = "ac6-agent-observation/v1"
RECEIPT_SCHEMA = "ac6-agent-episode-receipt/v1"
V2_SCHEMA = "emu-agent/v2"
_HEX = set("0123456789abcdef")
XEX_SHA256 = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
TARGET_ID = "ac6-demo-xbox360-pal"
_ACTION_KEYS = {"schema", "action_id", "session_id", "sequence", "tick", "xinput"}
_OBS_KEYS = {"schema", "observation_id", "session_id", "sequence", "tick", "present", "milestones", "domains", "availability", "provenance"}
_RECEIPT_KEYS = {"schema", "receipt_id", "session_id", "status", "backend", "qualified", "actions_sha256", "observations_sha256", "events", "error", "parent_receipt_id", "identity", "artifacts", "first_divergence", "stop_reason"}
_RECEIPT_ARTIFACT_KEYS = {"kind", "sha256", "size"}
_RECEIPT_ARTIFACT_KINDS = {"actions", "observations", "readback", "journal"}
_RECEIPT_STOP_REASONS = {"checkpoint", "backend_unavailable", "client_close", "error", "divergence", "limit"}
_TYPES = {"wait", "input", "advance", "pause", "resume"}
_CONTROLS = {"confirm", "cancel", "start", "select", "up", "down", "left", "right", "pause", "resume", "fire", "special"}
_DOMAINS = ("player", "camera", "flight", "target", "objective", "terminal", "readback")
_AVAILABLE_VALUE_KEYS = {
    "player": {"position", "health", "state"},
    "camera": {"position", "orientation", "fov"},
    "flight": {"speed", "altitude", "heading"},
    "target": {"id", "distance", "locked"},
    "objective": {"id", "state", "progress"},
    "terminal": {"state", "code"},
    "readback": {"sha256", "width", "height", "format"},
}
_AVAILABLE_PROVENANCE_KEYS = {"status", "reason", "target_id", "xex_sha256", "module", "source_kind", "artifact_sha256"}
_AVAILABLE_SOURCE_KINDS = {"demo-recomp", "demo-native"}
# Promotion of an available observation requires adding the SHA-256 of a
# locally verified PAL evidence artefact to this explicit registry.  It starts
# empty: transport input alone can never promote itself to qualified evidence.
QUALIFIED_PAL_ARTIFACT_SHA256S: frozenset[str] = frozenset()


def _obj(value: Any, name: str, keys: set[str]) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        raise ValidationError("must be an object", path=name)
    unknown = sorted(set(value) - keys)
    if unknown:
        raise ValidationError("unsupported field(s): " + ", ".join(map(str, unknown)), path=name)
    return dict(value)


def _text(value: Any, path: str, *, max_len: int = 128) -> str:
    if not isinstance(value, str) or not value or len(value) > max_len or "\x00" in value:
        raise ValidationError("must be a bounded non-empty string", path=path)
    return value


def _id(value: Any, path: str) -> str:
    value = _text(value, path)
    if any(ch not in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:-" for ch in value):
        raise ValidationError("must be an opaque identifier", path=path)
    return value


def _safe_json(value: Any, path: str = "$") -> Any:
    if isinstance(value, (str, int, bool)) or value is None:
        if isinstance(value, str) and len(value) > 256:
            raise ValidationError("string is too long", path=path)
        return value
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValidationError("floating point value must be finite", path=path)
        return value
    if isinstance(value, list):
        if len(value) > 64:
            raise ValidationError("array is too large", path=path)
        return [_safe_json(item, f"{path}[{i}]") for i, item in enumerate(value)]
    if isinstance(value, Mapping):
        if len(value) > 64:
            raise ValidationError("object is too large", path=path)
        return {str(key): _safe_json(item, f"{path}.{key}") for key, item in value.items()}
    raise ValidationError("value is not bounded JSON", path=path)


def validate_action(value: Any) -> dict[str, Any]:
    item = _obj(value, "$", _ACTION_KEYS)
    if item.get("schema") != ACTION_SCHEMA:
        raise ValidationError("unsupported action schema", path="$.schema")
    for key in ("action_id", "session_id"):
        _id(item.get(key), f"$.{key}")
    seq = item.get("sequence"); tick = item.get("tick")
    if isinstance(seq, bool) or not isinstance(seq, int) or not 0 <= seq <= 0xFFFFFFFF:
        raise ValidationError("must be a non-negative integer", path="$.sequence")
    if isinstance(tick, bool) or not isinstance(tick, int) or not 0 <= tick <= 0xFFFFFFFF:
        raise ValidationError("must be a bounded integer", path="$.tick")
    xinput = item.get("xinput")
    if not isinstance(xinput, Mapping) or set(xinput) != {"buttons", "left_trigger", "right_trigger", "left_stick", "right_stick", "connected"}:
        raise ValidationError("xinput frame is malformed", path="$.xinput")
    if isinstance(xinput["buttons"], bool) or not isinstance(xinput["buttons"], int) or not 0 <= xinput["buttons"] <= 0xFFFF:
        raise ValidationError("buttons must be uint16", path="$.xinput.buttons")
    for key in ("left_trigger", "right_trigger"):
        if isinstance(xinput[key], bool) or not isinstance(xinput[key], int) or not 0 <= xinput[key] <= 255:
            raise ValidationError("trigger must be uint8", path=f"$.xinput.{key}")
    for key in ("left_stick", "right_stick"):
        stick = xinput[key]
        if not isinstance(stick, Mapping) or set(stick) != {"x", "y"} or any(isinstance(v, bool) or not isinstance(v, int) or not -32768 <= v <= 32767 for v in stick.values()):
            raise ValidationError("stick must contain bounded int16 x/y", path=f"$.xinput.{key}")
    if not isinstance(xinput["connected"], bool):
        raise ValidationError("connected must be boolean", path="$.xinput.connected")
    return copy.deepcopy(item)


def unavailable_observation(session_id: str, sequence: int = 0, *, reason: str = "backend_unavailable") -> dict[str, Any]:
    domains = {domain: {"availability": "unavailable", "provenance": {"status": "unavailable", "reason": reason}, "value": None} for domain in _DOMAINS}
    return {"schema": OBSERVATION_SCHEMA, "observation_id": f"obs-{session_id}-{sequence}", "session_id": session_id, "sequence": sequence, "tick": sequence, "present": None, "milestones": [], "domains": domains, "availability": "unavailable", "provenance": {"status": "unavailable", "reason": reason}}


def _available_domain_value(name: str, value: Any, path: str) -> None:
    """Accept only the small, transport-safe value vocabulary for a domain."""

    if not isinstance(value, Mapping) or not value or set(value) - _AVAILABLE_VALUE_KEYS[name]:
        raise ValidationError("available domain value has an invalid shape", path=path)
    safe = _safe_json(value, path)
    if name in {"player", "camera"}:
        for key in ("position", "orientation"):
            if key in safe and (not isinstance(safe[key], list) or len(safe[key]) != 3 or any(isinstance(v, bool) or not isinstance(v, int) or not -1_000_000 <= v <= 1_000_000 for v in safe[key])):
                raise ValidationError("vector must contain three bounded integers", path=f"{path}.{key}")
    if name == "player":
        if "health" in safe and (isinstance(safe["health"], bool) or not isinstance(safe["health"], (int, float)) or not math.isfinite(safe["health"]) or not 0 <= safe["health"] <= 1_000_000):
            raise ValidationError("health must be a finite number in [0, 1000000]", path=f"{path}.health")
        if "state" in safe:
            _text(safe["state"], f"{path}.state", max_len=64)
    if name == "camera" and "fov" in safe and (isinstance(safe["fov"], bool) or not isinstance(safe["fov"], (int, float)) or not math.isfinite(safe["fov"]) or not 1 <= safe["fov"] <= 179):
        raise ValidationError("fov must be a finite number in [1, 179]", path=f"{path}.fov")
    elif name == "flight":
        for key in ("speed", "altitude", "heading"):
            if key in safe and (isinstance(safe[key], bool) or not isinstance(safe[key], int) or not -1_000_000 <= safe[key] <= 1_000_000):
                raise ValidationError("flight value must be a bounded integer", path=f"{path}.{key}")
    elif name == "target":
        if "id" in safe:
            _id(safe["id"], f"{path}.id")
        if "distance" in safe and (isinstance(safe["distance"], bool) or not isinstance(safe["distance"], int) or not 0 <= safe["distance"] <= 1_000_000):
            raise ValidationError("distance must be a bounded integer", path=f"{path}.distance")
        if "locked" in safe and not isinstance(safe["locked"], bool):
            raise ValidationError("locked must be boolean", path=f"{path}.locked")
    elif name in {"objective", "terminal"}:
        for key, text in safe.items():
            if key != "progress" and not isinstance(text, str):
                raise ValidationError("status value must be text", path=f"{path}.{key}")
            if key == "progress" and (isinstance(text, bool) or not isinstance(text, int) or not 0 <= text <= 100):
                raise ValidationError("progress must be a percentage", path=f"{path}.{key}")
    elif name == "readback":
        if set(safe) != {"sha256", "width", "height", "format"} or not isinstance(safe["sha256"], str) or len(safe["sha256"]) != 64 or any(ch not in _HEX for ch in safe["sha256"]) or isinstance(safe["width"], bool) or not isinstance(safe["width"], int) or isinstance(safe["height"], bool) or not isinstance(safe["height"], int) or not 1 <= safe["width"] <= 8192 or not 1 <= safe["height"] <= 8192 or safe["format"] not in {"rgba8", "bgra8"}:
            raise ValidationError("readback value is malformed", path=path)


def _validate_provenance(value: Any, availability: str, path: str) -> None:
    if not isinstance(value, Mapping):
        raise ValidationError("provenance must be an object", path=path)
    if availability == "unavailable":
        if set(value) != {"status", "reason"} or value.get("status") != "unavailable":
            raise ValidationError("unavailable provenance is malformed", path=path)
    elif set(value) != _AVAILABLE_PROVENANCE_KEYS or value.get("status") != "qualified" or value.get("target_id") != TARGET_ID or value.get("xex_sha256") != XEX_SHA256 or value.get("module") != "Default.xex" or value.get("source_kind") not in _AVAILABLE_SOURCE_KINDS:
        raise ValidationError("available provenance is not qualified AC6 PAL evidence", path=path)
    if not isinstance(value.get("reason"), str) or len(value["reason"]) > 128:
        raise ValidationError("provenance reason is malformed", path=f"{path}.reason")
    if availability == "available":
        artifact = value.get("artifact_sha256")
        if not isinstance(artifact, str) or len(artifact) != 64 or any(ch not in _HEX for ch in artifact):
            raise ValidationError("artifact_sha256 must be a lowercase SHA-256", path=f"{path}.artifact_sha256")
        if artifact not in QUALIFIED_PAL_ARTIFACT_SHA256S:
            raise ValidationError("artifact_sha256 is not a locally qualified PAL evidence artefact", path=f"{path}.artifact_sha256")


def validate_observation(value: Any) -> dict[str, Any]:
    item = _obj(value, "$", _OBS_KEYS)
    if item.get("schema") != OBSERVATION_SCHEMA:
        raise ValidationError("unsupported observation schema", path="$.schema")
    _id(item.get("observation_id"), "$.observation_id"); _id(item.get("session_id"), "$.session_id")
    if item.get("availability") not in {"available", "unavailable"}:
        raise ValidationError("availability must be available or unavailable", path="$.availability")
    for key in ("sequence", "tick"):
        if isinstance(item.get(key), bool) or not isinstance(item.get(key), int) or not 0 <= item[key] <= 0xFFFFFFFF:
            raise ValidationError("must be a bounded integer", path=f"$.{key}")
    if item.get("present") is not None and (isinstance(item["present"], bool) or not isinstance(item["present"], int) or not 0 <= item["present"] <= 0xFFFFFFFFFFFFFFFF):
        raise ValidationError("present must be a non-negative integer or null", path="$.present")
    if not isinstance(item.get("milestones"), list) or len(item["milestones"]) > 128 or any(not isinstance(x, str) or len(x) > 64 for x in item["milestones"]):
        raise ValidationError("milestones are malformed", path="$.milestones")
    _validate_provenance(item.get("provenance"), item["availability"], "$.provenance")
    domains = item.get("domains")
    if not isinstance(domains, Mapping) or set(domains) != set(_DOMAINS):
        raise ValidationError("domains must contain the fixed AC6 domain set", path="$.domains")
    for name, domain in domains.items():
        if not isinstance(domain, Mapping) or set(domain) != {"availability", "provenance", "value"}:
            raise ValidationError("domain envelope is malformed", path=f"$.domains.{name}")
        if domain["availability"] not in {"available", "unavailable"}:
            raise ValidationError("domain availability/provenance is malformed", path=f"$.domains.{name}")
        _validate_provenance(domain["provenance"], domain["availability"], f"$.domains.{name}.provenance")
        if domain["availability"] == "unavailable" and domain["value"] is not None:
            raise ValidationError("unavailable domain must not contain a value", path=f"$.domains.{name}.value")
        if domain["availability"] == "available":
            _available_domain_value(name, domain["value"], f"$.domains.{name}.value")
        if item["availability"] == "unavailable" and domain["availability"] != "unavailable":
            raise ValidationError("unavailable observation cannot contain available domains", path=f"$.domains.{name}.availability")
    if item["availability"] == "available" and not any(domain["availability"] == "available" for domain in domains.values()):
        raise ValidationError("available observation requires an available domain", path="$.availability")
    return copy.deepcopy(item)


def validate_receipt(value: Any) -> dict[str, Any]:
    item = _obj(value, "$", _RECEIPT_KEYS)
    if item.get("schema") != RECEIPT_SCHEMA:
        raise ValidationError("unsupported receipt schema", path="$.schema")
    for key in ("receipt_id", "session_id"):
        _id(item.get(key), f"$.{key}")
    if item.get("status") not in {"completed", "backend_unavailable", "closed", "error"}:
        raise ValidationError("unsupported receipt status", path="$.status")
    if not isinstance(item.get("qualified"), bool) or not isinstance(item.get("events"), list):
        raise ValidationError("receipt status fields are malformed", path="$")
    if item.get("stop_reason") not in _RECEIPT_STOP_REASONS:
        raise ValidationError("unsupported receipt stop reason", path="$.stop_reason")
    artifacts = item.get("artifacts")
    if not isinstance(artifacts, list) or len(artifacts) > 64:
        raise ValidationError("receipt artifacts are malformed", path="$.artifacts")
    for index, artifact in enumerate(artifacts):
        if not isinstance(artifact, Mapping) or set(artifact) != _RECEIPT_ARTIFACT_KEYS:
            raise ValidationError("receipt artifact reference is malformed", path=f"$.artifacts[{index}]")
        if artifact["kind"] not in _RECEIPT_ARTIFACT_KINDS:
            raise ValidationError("receipt artifact kind is unsupported", path=f"$.artifacts[{index}].kind")
        digest_value = artifact["sha256"]
        if not isinstance(digest_value, str) or len(digest_value) != 64 or any(ch not in _HEX for ch in digest_value):
            raise ValidationError("receipt artifact sha256 is malformed", path=f"$.artifacts[{index}].sha256")
        if isinstance(artifact["size"], bool) or not isinstance(artifact["size"], int) or not 0 <= artifact["size"] <= 256 * 1024 * 1024:
            raise ValidationError("receipt artifact size is malformed", path=f"$.artifacts[{index}].size")
    divergence = item.get("first_divergence")
    if divergence is not None:
        divergence = _obj(divergence, "$.first_divergence", {"sequence", "field", "left", "right"})
        if isinstance(divergence["sequence"], bool) or not isinstance(divergence["sequence"], int) or not 0 <= divergence["sequence"] <= 0xFFFFFFFF:
            raise ValidationError("first divergence sequence is malformed", path="$.first_divergence.sequence")
        _text(divergence["field"], "$.first_divergence.field", max_len=64)
        _safe_json(divergence["left"], "$.first_divergence.left")
        _safe_json(divergence["right"], "$.first_divergence.right")
    if item["qualified"]:
        raise ValidationError("unavailable backend cannot produce a qualified receipt", path="$.qualified")
    if item["status"] == "backend_unavailable" and not item.get("error"):
        raise ValidationError("backend_unavailable receipt requires an error", path="$.error")
    for key in ("actions_sha256", "observations_sha256"):
        digest = item.get(key)
        if not isinstance(digest, str) or len(digest) != 64 or any(ch not in _HEX for ch in digest):
            raise ValidationError("must be a lowercase SHA-256", path=f"$.{key}")
    identity = item.get("identity")
    if not isinstance(item.get("backend"), str) or item["backend"] not in {"demo-recomp", "demo-native"}:
        raise ValidationError("backend is not a v2 backend", path="$.backend")
    if item.get("error") is not None and (not isinstance(item["error"], str) or len(item["error"]) > 256):
        raise ValidationError("error must be bounded text or null", path="$.error")
    if item.get("parent_receipt_id") is not None:
        _id(item["parent_receipt_id"], "$.parent_receipt_id")
    if not isinstance(identity, Mapping) or set(identity) != {"target_id", "xex_sha256", "module"} or identity.get("target_id") != TARGET_ID or identity.get("xex_sha256") != XEX_SHA256 or identity.get("module") != "Default.xex":
        raise ValidationError("receipt identity is not the qualified AC6 demo", path="$.identity")
    if len(item["events"]) > 4096:
        raise ValidationError("events exceed receipt limit", path="$.events")
    for index, event in enumerate(item["events"]):
        if not isinstance(event, Mapping) or set(event) != {"type", "sequence"} or event["type"] != "step" or isinstance(event["sequence"], bool) or not isinstance(event["sequence"], int) or not 0 <= event["sequence"] <= 0xFFFFFFFF:
            raise ValidationError("event is malformed", path=f"$.events[{index}]")
    return copy.deepcopy(item)


def digest(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()).hexdigest()
