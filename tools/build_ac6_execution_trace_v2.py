#!/usr/bin/env python3
"""Seal ordered AC6 Mission 01 JSONL events as ac6.execution-trace.v2."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import stat
import struct
import tempfile
from pathlib import Path
from typing import Any


TRACE_SCHEMA = "ac6.execution-trace.v2"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
DOMAINS = (
    "controller_input",
    "simulation_snapshot",
    "mission_objectives",
    "graphics_submission",
    "output_hashes",
)
EVENT_KEYS = {"sequence", "tick", "domain", "payload"}
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")
HEX32 = re.compile(r"[0-9A-F]{8}")
HEX64 = re.compile(r"[0-9a-f]{16}")
XEX_VERSION = re.compile(r"v(?:0|[1-9][0-9]{0,9})(?:\.(?:0|[1-9][0-9]{0,9})){3}")
MAX_TRACE_TICKS = 600_000
MAX_PROJECTED_FRAMES = 1_000_000
MAX_SOURCE_MARKERS = 500_000
MAX_RAW_BYTES = 1024 * 1024 * 1024
MAX_TRACE_BYTES = 1024 * 1024 * 1024
MAX_LINE_BYTES = 1024 * 1024
MAX_RECEIPT_BYTES = 64 * 1024
MAX_ARTIFACT_BYTES = 512 * 1024 * 1024
MAX_TOTAL_INPUT_BYTES = 1024 * 1024 * 1024
MAX_VALUE_DEPTH = 64
MAX_VALUE_NODES = 100_000
MAX_CONTAINER_ITEMS = 100_000
MAX_STRING_BYTES = 1024 * 1024
MAX_JSON_INTEGER_DIGITS = 20
PROJECTION_RECEIPT_SCHEMA = "ac6.native-controller-projection-receipt.v3"
CADENCE_CENSUS_SCHEMA = "ac6.controller-cadence-census.v1"
CADENCE_METHOD = "uniform_marker_interval_v1"
INTEGRITY_ONLY_CENSUS = "integrity_only_runtime_census"
RUNNER_ATTESTED = "runner_attested"
NATIVE_CLOCK_CONTRACT = {
    "schema": "ac6.native-simulation-clock.v1",
    "clock_id": "ac6_native_fixed_step",
    "frequency": {"numerator": 60, "denominator": 1},
    "tick_semantics": "one_simulation_step",
}
NATIVE_SIMULATION_HZ = 60
SUPPORTED_CADENCES = {(30, 60, 2), (60, 60, 1)}
MARKER_ROLES = {"ac6_frame_input_stage", "mission_manager_tick"}
MARKER_PHASES = {"before_input", "after_input"}
CONTROLLER_FIELDS = {"pitch", "roll", "yaw", "throttle", "buttons"}
CONTROLLER_MAPPING = {
    "pitch": "thumb_ly",
    "roll": "thumb_lx",
    "yaw": "left_shoulder=-32768;right_shoulder=32767;otherwise=thumb_rx;left_precedes_right",
    "throttle": "right_trigger",
    "buttons": "raw_xinput_buttons",
}
RECEIPT_KEYS = {
    "kind",
    "schema",
    "source",
    "target",
    "cadence",
    "mapping",
    "cache_index_sha256",
    "output",
}
RECEIPT_SOURCE_KEYS = {
    "raw_replay_sha256",
    "raw_payload_sha256",
    "parent_replay_sha256",
    "parent_payload_sha256",
    "parent_window",
}
RECEIPT_TARGET_KEYS = {
    "title_id",
    "media_id",
    "module",
    "xex_sha256",
    "xex_version",
    "base_version",
    "module_xxh3",
    "marker_address",
    "marker_code_offset",
    "marker_code_length",
    "marker_code_sha256",
}
RECEIPT_OUTPUT_KEYS = {
    "format",
    "version",
    "mission_id",
    "difficulty",
    "difficulty_name",
    "aircraft_id",
    "weapon_id",
    "capability_data_valid",
    "random_seed",
    "checkpoint_count",
    "source_marker_count",
    "frame_count",
    "final_tick",
    "input_digest_sha256",
    "final_digest_sha256",
    "output_sha256",
}
CENSUS_REFERENCE_KEYS = {
    "schema",
    "file_sha256",
    "payload_sha256",
    "integrity_level",
    "method",
    "record_count",
    "interval_count",
}
MARKER_CONTRACT_KEYS = {"role", "address", "phase", "code"}
MARKER_CODE_KEYS = {"offset", "length", "sha256"}
NATIVE_CLOCK_KEYS = {"schema", "clock_id", "frequency", "tick_semantics"}
RATIONAL_KEYS = {"numerator", "denominator"}


class TraceV2Error(ValueError):
    pass


def _reject_json_constant(value: str) -> None:
    raise TraceV2Error(f"non-finite JSON number: {value}")


def _parse_json_int(value: str) -> int:
    if len(value.removeprefix("-")) > MAX_JSON_INTEGER_DIGITS:
        raise TraceV2Error("JSON integer bound")
    return int(value)


def _validate_json_nesting(data: bytes, name: str) -> None:
    depth = 0
    in_string = False
    escaped = False
    for byte in data:
        if in_string:
            if escaped:
                escaped = False
            elif byte == 0x5C:
                escaped = True
            elif byte == 0x22:
                in_string = False
            continue
        if byte == 0x22:
            in_string = True
        elif byte in (0x5B, 0x7B):
            depth += 1
            if depth > MAX_VALUE_DEPTH:
                raise TraceV2Error(f"{name} depth bound")
        elif byte in (0x5D, 0x7D):
            depth -= 1
            if depth < 0:
                raise TraceV2Error(f"{name} nesting")
    if in_string or depth != 0:
        raise TraceV2Error(f"{name} nesting")


def _validate_value_bounds(value: object, name: str) -> None:
    pending: list[tuple[object, int]] = [(value, 0)]
    nodes = 0
    while pending:
        current, depth = pending.pop()
        nodes += 1
        if nodes > MAX_VALUE_NODES:
            raise TraceV2Error(f"{name} node bound")
        if depth > MAX_VALUE_DEPTH:
            raise TraceV2Error(f"{name} depth bound")
        if isinstance(current, dict):
            if len(current) > MAX_CONTAINER_ITEMS:
                raise TraceV2Error(f"{name} member bound")
            for key, child in current.items():
                if not isinstance(key, str) or len(key.encode()) > MAX_STRING_BYTES:
                    raise TraceV2Error(f"{name} key bound")
                pending.append((child, depth + 1))
        elif isinstance(current, list):
            if len(current) > MAX_CONTAINER_ITEMS:
                raise TraceV2Error(f"{name} item bound")
            pending.extend((child, depth + 1) for child in current)
        elif isinstance(current, str) and len(current.encode()) > MAX_STRING_BYTES:
            raise TraceV2Error(f"{name} string bound")
        elif isinstance(current, float) and not math.isfinite(current):
            raise TraceV2Error(f"{name} non-finite number")


def sha256(path: Path) -> str:
    return hashlib.sha256(_read_bounded(path, MAX_ARTIFACT_BYTES, "artifact")).hexdigest()


def require_dict(value: object, keys: set[str], name: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != keys:
        raise TraceV2Error(f"{name} shape")
    return value


def require_uint(value: object, maximum: int, name: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= maximum:
        raise TraceV2Error(f"{name} range")
    return value


def require_sha256(value: object, name: str) -> str:
    if not isinstance(value, str) or SHA256.fullmatch(value) is None:
        raise TraceV2Error(f"{name} sha256")
    return value


def _read_bounded(
    path: Path,
    maximum_bytes: int,
    name: str,
    *,
    total_limited: bool = False,
) -> bytes:
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NONBLOCK)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise TraceV2Error(f"{name} is not a regular file")
        with os.fdopen(descriptor, "rb") as source:
            descriptor = -1
            data = source.read(maximum_bytes + 1)
            if len(data) > maximum_bytes or source.read(1):
                reason = "total input byte bound" if total_limited else f"{name} byte bound"
                raise TraceV2Error(reason)
        return data
    except OSError as error:
        raise TraceV2Error(f"{name} unreadable: {error}") from error
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def _paths_alias(left: Path, right: Path) -> bool:
    if left.resolve(strict=False) == right.resolve(strict=False):
        return True
    try:
        return left.exists() and right.exists() and left.samefile(right)
    except OSError:
        return False


def _require_output_not_input(output: Path, inputs: tuple[Path, ...]) -> None:
    if any(_paths_alias(output, source) for source in inputs):
        raise TraceV2Error("output aliases an input")


def _atomic_write(output: Path, data: bytes) -> None:
    descriptor = -1
    temporary: Path | None = None
    try:
        descriptor, temporary_name = tempfile.mkstemp(prefix=f".{output.name}.", suffix=".tmp", dir=output.parent)
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "wb") as stream:
            descriptor = -1
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
        temporary = None
    finally:
        if descriptor >= 0:
            os.close(descriptor)
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def validate_events(events: object, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    if not isinstance(events, list) or len(events) != tick_count * len(DOMAINS):
        raise TraceV2Error("event count")
    for sequence, event in enumerate(events):
        _validate_value_bounds(event, f"event {sequence}")
        if not isinstance(event, dict) or set(event) != EVENT_KEYS:
            raise TraceV2Error(f"event {sequence} shape")
        expected_tick = start_tick + sequence // len(DOMAINS)
        expected_domain = DOMAINS[sequence % len(DOMAINS)]
        if (
            not isinstance(event["sequence"], int)
            or isinstance(event["sequence"], bool)
            or event["sequence"] != sequence
        ):
            raise TraceV2Error(f"event {sequence} sequence")
        if not isinstance(event["tick"], int) or isinstance(event["tick"], bool) or event["tick"] != expected_tick:
            raise TraceV2Error(f"event {sequence} tick")
        if event["domain"] != expected_domain:
            raise TraceV2Error(f"event {sequence} domain")
        if not isinstance(event["payload"], dict):
            raise TraceV2Error(f"event {sequence} payload")
        if expected_domain == "output_hashes":
            if not event["payload"] or any(
                not isinstance(name, str) or not name or not isinstance(value, str) or SHA256.fullmatch(value) is None
                for name, value in event["payload"].items()
            ):
                raise TraceV2Error(f"event {sequence} output hashes")
    return events


def load_jsonl(path: Path, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    return load_jsonl_bytes(_read_bounded(path, MAX_RAW_BYTES, "raw capture"), start_tick, tick_count)


def load_jsonl_bytes(data: bytes, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    if tick_count <= 0 or tick_count > MAX_PROJECTED_FRAMES:
        raise TraceV2Error("raw tick count bound")
    maximum_events = tick_count * len(DOMAINS)
    events: list[dict[str, Any]] = []
    for line_number, raw_line in enumerate(data.splitlines(keepends=True), 1):
        if len(raw_line) > MAX_LINE_BYTES:
            raise TraceV2Error(f"line {line_number}: byte bound")
        if not raw_line.strip():
            continue
        if len(events) >= maximum_events:
            raise TraceV2Error("event count")
        try:
            _validate_json_nesting(raw_line, f"line {line_number}")
            events.append(
                json.loads(
                    raw_line,
                    parse_int=_parse_json_int,
                    parse_constant=_reject_json_constant,
                )
            )
        except UnicodeDecodeError as error:
            raise TraceV2Error(f"line {line_number}: utf-8") from error
        except json.JSONDecodeError as error:
            raise TraceV2Error(f"line {line_number}: {error.msg}") from error
    return validate_events(events, start_tick, tick_count)


def _validate_projection_receipt(document: object) -> dict[str, Any]:
    receipt = require_dict(document, RECEIPT_KEYS, "projection receipt")
    if receipt["kind"] != "native_projection_receipt" or receipt["schema"] != PROJECTION_RECEIPT_SCHEMA:
        raise TraceV2Error("projection receipt identity")

    source = require_dict(receipt["source"], RECEIPT_SOURCE_KEYS, "projection source")
    for key in (
        "raw_replay_sha256",
        "raw_payload_sha256",
        "parent_replay_sha256",
        "parent_payload_sha256",
    ):
        require_sha256(source[key], f"projection source {key}")
    parent_window = require_dict(source["parent_window"], {"start_marker", "marker_count"}, "projection parent window")
    parent_start = require_uint(parent_window["start_marker"], MAX_SOURCE_MARKERS, "projection parent start")
    parent_count = require_uint(parent_window["marker_count"], MAX_SOURCE_MARKERS, "projection parent count")
    if parent_start == 0 or parent_count == 0 or parent_start > MAX_SOURCE_MARKERS - parent_count + 1:
        raise TraceV2Error("projection parent window range")

    target = require_dict(receipt["target"], RECEIPT_TARGET_KEYS, "projection target")
    if (
        target["module"] != "default.xex"
        or target["xex_sha256"] != XEX_SHA256
        or target["title_id"] != "4E4D07D1"
        or target["media_id"] != "0379EFB3"
        or target["xex_version"] != "v0.0.0.11"
        or target["base_version"] != "v0.0.0.11"
        or not isinstance(target["title_id"], str)
        or HEX32.fullmatch(target["title_id"]) is None
        or not isinstance(target["media_id"], str)
        or HEX32.fullmatch(target["media_id"]) is None
        or not isinstance(target["module_xxh3"], str)
        or HEX64.fullmatch(target["module_xxh3"]) is None
        or not isinstance(target["marker_address"], str)
        or HEX32.fullmatch(target["marker_address"]) is None
    ):
        raise TraceV2Error("projection target identity")
    marker_code_offset = require_uint(target["marker_code_offset"], 0xFFFFFFFF, "projection target marker code offset")
    marker_code_length = require_uint(target["marker_code_length"], 4096, "projection target marker code length")
    if marker_code_length == 0 or marker_code_offset > 0xFFFFFFFF - marker_code_length + 1:
        raise TraceV2Error("projection target marker code range")
    for key in ("xex_version", "base_version"):
        if not isinstance(target[key], str) or XEX_VERSION.fullmatch(target[key]) is None:
            raise TraceV2Error(f"projection target {key}")
    require_sha256(target["marker_code_sha256"], "projection target marker code")

    cadence = require_dict(
        receipt["cadence"],
        {
            "integrity_level",
            "source_hz",
            "native_hz",
            "resampling",
            "hold",
            "census",
            "marker_contract",
            "native_clock",
        },
        "projection cadence",
    )
    source_hz = require_uint(cadence["source_hz"], 1000, "projection source_hz")
    native_hz = require_uint(cadence["native_hz"], 1000, "projection native_hz")
    hold = require_uint(cadence["hold"], 1000, "projection hold")
    if native_hz != NATIVE_SIMULATION_HZ or source_hz == 0 or native_hz % source_hz or hold != native_hz // source_hz:
        raise TraceV2Error("projection cadence unsupported")
    expected_resampling = "identity" if hold == 1 else "zero_order_hold"
    if cadence["resampling"] != expected_resampling:
        raise TraceV2Error("projection cadence resampling")
    if cadence["integrity_level"] == RUNNER_ATTESTED:
        raise TraceV2Error("runner attestation verification is unavailable")
    if cadence["integrity_level"] != INTEGRITY_ONLY_CENSUS:
        raise TraceV2Error("projection cadence integrity level")
    census = require_dict(cadence["census"], CENSUS_REFERENCE_KEYS, "projection cadence census")
    record_count = require_uint(census["record_count"], MAX_SOURCE_MARKERS, "projection census records")
    interval_count = require_uint(census["interval_count"], MAX_SOURCE_MARKERS - 1, "projection census intervals")
    if (
        census["schema"] != CADENCE_CENSUS_SCHEMA
        or census["integrity_level"] != INTEGRITY_ONLY_CENSUS
        or census["method"] != CADENCE_METHOD
        or record_count < 2
        or interval_count != record_count - 1
    ):
        raise TraceV2Error("projection cadence census identity")
    require_sha256(census["file_sha256"], "projection cadence census file")
    require_sha256(census["payload_sha256"], "projection cadence census payload")
    marker_contract = require_dict(cadence["marker_contract"], MARKER_CONTRACT_KEYS, "projection marker contract")
    if (
        marker_contract["role"] not in MARKER_ROLES
        or marker_contract["phase"] not in MARKER_PHASES
        or not isinstance(marker_contract["address"], str)
        or HEX32.fullmatch(marker_contract["address"]) is None
        or marker_contract["address"] != target["marker_address"]
    ):
        raise TraceV2Error("projection marker contract identity")
    marker_code = require_dict(marker_contract["code"], MARKER_CODE_KEYS, "projection marker code")
    code_offset = require_uint(marker_code["offset"], 0xFFFFFFFF, "projection marker code offset")
    code_length = require_uint(marker_code["length"], 4096, "projection marker code length")
    require_sha256(marker_code["sha256"], "projection marker code")
    if (
        code_length == 0
        or code_offset > 0xFFFFFFFF - code_length + 1
        or code_offset != marker_code_offset
        or code_length != marker_code_length
        or marker_code["sha256"] != target["marker_code_sha256"]
    ):
        raise TraceV2Error("projection marker contract code mismatch")
    native_clock = require_dict(cadence["native_clock"], NATIVE_CLOCK_KEYS, "projection native clock")
    frequency = require_dict(native_clock["frequency"], RATIONAL_KEYS, "projection native clock frequency")
    native_numerator = require_uint(
        frequency["numerator"],
        0xFFFFFFFFFFFFFFFF,
        "projection native clock numerator",
    )
    native_denominator = require_uint(
        frequency["denominator"],
        0xFFFFFFFFFFFFFFFF,
        "projection native clock denominator",
    )
    if (
        native_clock["schema"] != NATIVE_CLOCK_CONTRACT["schema"]
        or native_clock["clock_id"] != NATIVE_CLOCK_CONTRACT["clock_id"]
        or native_clock["tick_semantics"] != NATIVE_CLOCK_CONTRACT["tick_semantics"]
        or native_numerator != 60
        or native_denominator != 1
    ):
        raise TraceV2Error("projection native clock contract")
    if receipt["mapping"] != CONTROLLER_MAPPING:
        raise TraceV2Error("projection controller mapping")
    cache_index = require_sha256(receipt["cache_index_sha256"], "projection cache index")
    if set(cache_index) == {"0"}:
        raise TraceV2Error("projection cache index zero")

    output = require_dict(receipt["output"], RECEIPT_OUTPUT_KEYS, "projection output")
    fixed_output = {
        "format": "AC6RTPLY",
        "version": 3,
        "mission_id": 1,
        "difficulty": 1,
        "difficulty_name": "Normal",
        "aircraft_id": 1,
        "weapon_id": 1,
        "capability_data_valid": True,
        "checkpoint_count": 0,
    }
    if any(type(output[key]) is not type(value) or output[key] != value for key, value in fixed_output.items()):
        raise TraceV2Error("projection output identity")
    random_seed = require_uint(output["random_seed"], 0xFFFFFFFFFFFFFFFF, "projection random seed")
    source_markers = require_uint(output["source_marker_count"], MAX_SOURCE_MARKERS, "projection source markers")
    frame_count = require_uint(output["frame_count"], MAX_PROJECTED_FRAMES, "projection frame count")
    final_tick = require_uint(output["final_tick"], MAX_PROJECTED_FRAMES, "projection final tick")
    if (
        random_seed == 0
        or source_markers == 0
        or source_markers != parent_count
        or source_markers != record_count
        or source_markers > MAX_PROJECTED_FRAMES // hold
        or frame_count != source_markers * hold
        or final_tick != frame_count
    ):
        raise TraceV2Error("projection output bounds")
    input_digest = require_sha256(output["input_digest_sha256"], "projection input digest")
    final_digest = require_sha256(output["final_digest_sha256"], "projection final digest")
    require_sha256(output["output_sha256"], "projection output")
    if input_digest != final_digest:
        raise TraceV2Error("projection digest mismatch")
    return receipt


def _require_trace_grade_receipt(receipt: dict[str, Any]) -> None:
    integrity_level = receipt["cadence"]["integrity_level"]
    if integrity_level == INTEGRITY_ONLY_CENSUS:
        raise TraceV2Error("integrity-only runtime census is not trace-grade evidence")
    if integrity_level == RUNNER_ATTESTED:
        raise TraceV2Error("runner attestation verification is unavailable")
    raise TraceV2Error("projection cadence integrity level")


def _load_projection_receipt_snapshot(path: Path) -> tuple[dict[str, Any], str]:
    data = _read_bounded(path, MAX_RECEIPT_BYTES, "projection receipt")
    try:
        _validate_json_nesting(data, "projection receipt")
        receipt = _validate_projection_receipt(
            json.loads(
                data,
                parse_int=_parse_json_int,
                parse_constant=_reject_json_constant,
            )
        )
    except TraceV2Error:
        raise
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError, ValueError) as error:
        raise TraceV2Error("projection receipt JSON") from error
    canonical = (json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n").encode()
    if data != canonical:
        raise TraceV2Error("projection receipt is not canonical")
    return receipt, hashlib.sha256(data).hexdigest()


def load_projection_receipt(path: Path) -> dict[str, Any]:
    receipt, _ = _load_projection_receipt_snapshot(path)
    return receipt


def validate_cadence(value: object, name: str) -> dict[str, Any]:
    cadence = require_dict(
        value,
        {
            "oracle_update_hz",
            "native_simulation_hz",
            "native_ticks_per_sample",
            "input_resampling",
            "snapshot_sampling",
        },
        name,
    )
    sample_hz = require_uint(cadence["oracle_update_hz"], 1000, f"{name} oracle hz")
    native_hz = require_uint(cadence["native_simulation_hz"], 1000, f"{name} native hz")
    hold = require_uint(cadence["native_ticks_per_sample"], 1000, f"{name} hold")
    if (sample_hz, native_hz, hold) not in SUPPORTED_CADENCES:
        raise TraceV2Error(f"{name} unsupported")
    if cadence["input_resampling"] != ("identity" if hold == 1 else "zero_order_hold"):
        raise TraceV2Error(f"{name} input resampling")
    if cadence["snapshot_sampling"] != "last_native_tick_in_sample":
        raise TraceV2Error(f"{name} snapshot sampling")
    return cadence


def validate_controller_replay_contract(value: object, name: str) -> dict[str, Any]:
    contract = require_dict(
        value,
        {
            "receipt",
            "source_replay_sha256",
            "source_payload_sha256",
            "projected_replay_sha256",
            "parent_replay_sha256",
            "parent_payload_sha256",
            "parent_window",
        },
        name,
    )
    receipt_artifact = contract["receipt"]
    if (
        not isinstance(receipt_artifact, dict)
        or set(receipt_artifact) != {"path", "sha256"}
        or not isinstance(receipt_artifact["path"], str)
        or not receipt_artifact["path"]
    ):
        raise TraceV2Error(f"{name} receipt")
    require_sha256(receipt_artifact["sha256"], f"{name} receipt")
    for key in (
        "source_replay_sha256",
        "source_payload_sha256",
        "projected_replay_sha256",
        "parent_replay_sha256",
        "parent_payload_sha256",
    ):
        require_sha256(contract[key], f"{name} {key}")
    parent = require_dict(contract["parent_window"], {"start_marker", "marker_count"}, f"{name} parent window")
    parent_start = require_uint(parent["start_marker"], MAX_SOURCE_MARKERS, f"{name} parent start")
    parent_count = require_uint(parent["marker_count"], MAX_SOURCE_MARKERS, f"{name} parent count")
    if parent_start == 0 or parent_count == 0 or parent_start > MAX_SOURCE_MARKERS - parent_count + 1:
        raise TraceV2Error(f"{name} parent window range")
    return contract


def validate_observation(
    value: object,
    cadence: dict[str, Any],
    source_marker_count: int,
    name: str,
) -> dict[str, Any]:
    observation = require_dict(
        value,
        {"logical_sample_hz", "oracle", "native"},
        name,
    )
    source_hz = cadence["oracle_update_hz"]
    native_hz = cadence["native_simulation_hz"]
    hold = cadence["native_ticks_per_sample"]
    raw_keys = {
        "raw_sample_hz",
        "raw_start_tick",
        "raw_tick_count",
        "selection",
        "selected_tick_stride",
        "selected_tick_phase",
    }
    expected_oracle = {
        "raw_sample_hz": source_hz,
        "raw_start_tick": 1,
        "raw_tick_count": source_marker_count,
        "selection": "identity",
        "selected_tick_stride": 1,
        "selected_tick_phase": 1,
    }
    expected_native = {
        "raw_sample_hz": native_hz,
        "raw_start_tick": 1,
        "raw_tick_count": source_marker_count * hold,
        "selection": "identity" if hold == 1 else "last_native_tick_in_sample",
        "selected_tick_stride": hold,
        "selected_tick_phase": hold,
    }
    if (
        observation["logical_sample_hz"] != source_hz
        or require_dict(observation["oracle"], raw_keys, f"{name} oracle") != expected_oracle
        or require_dict(observation["native"], raw_keys, f"{name} native") != expected_native
    ):
        raise TraceV2Error(f"{name} contract")
    return observation


def artifact(path: Path) -> dict[str, str]:
    data = _read_bounded(path, MAX_ARTIFACT_BYTES, "artifact")
    return {"path": path.as_posix(), "sha256": hashlib.sha256(data).hexdigest()}


def _artifact_snapshot(
    path: Path,
    name: str,
    maximum_bytes: int | None = None,
    *,
    total_limited: bool = False,
) -> tuple[dict[str, str], int]:
    limit = MAX_ARTIFACT_BYTES if maximum_bytes is None else maximum_bytes
    data = _read_bounded(path, limit, name, total_limited=total_limited)
    return {"path": path.as_posix(), "sha256": hashlib.sha256(data).hexdigest()}, len(data)


def _projected_replay_frames(path: Path, receipt: dict[str, Any]) -> bytes:
    output = receipt["output"]
    frame_count = output["frame_count"]
    expected_size = 121 + frame_count * 9
    data = _read_bounded(path, expected_size, "projected replay")
    if len(data) != expected_size or hashlib.sha256(data).hexdigest() != output["output_sha256"]:
        raise TraceV2Error("projected replay identity")
    if data[:9] != b"AC6RTPLY\0":
        raise TraceV2Error("projected replay magic")
    values = struct.unpack_from("<IIIIII", data, 9)
    if values != (3, 1, 1, 1, 1, 1):
        raise TraceV2Error("projected replay header")
    if data[33:65].hex() != receipt["cache_index_sha256"]:
        raise TraceV2Error("projected replay cache identity")
    random_seed, checkpoint_count = struct.unpack_from("<QI", data, 65)
    final_tick = struct.unpack_from("<Q", data, 77)[0]
    final_digest = data[85:117]
    stored_frame_count = struct.unpack_from("<I", data, 117)[0]
    frames = data[121:]
    if (
        random_seed != output["random_seed"]
        or checkpoint_count != 0
        or final_tick != frame_count
        or stored_frame_count != frame_count
        or hashlib.sha256(frames).digest() != final_digest
        or final_digest.hex() != output["input_digest_sha256"]
    ):
        raise TraceV2Error("projected replay payload")
    return frames


def _controller_payload(frame: bytes, offset: int) -> dict[str, int]:
    pitch, roll, yaw, throttle, buttons = struct.unpack_from("<hhhBH", frame, offset)
    return {
        "pitch": pitch,
        "roll": roll,
        "yaw": yaw,
        "throttle": throttle,
        "buttons": buttons,
    }


def _validate_controller_payload(value: object, name: str) -> dict[str, int]:
    payload = require_dict(value, CONTROLLER_FIELDS, name)
    bounds = {
        "pitch": (-32768, 32767),
        "roll": (-32768, 32767),
        "yaw": (-32768, 32767),
        "throttle": (0, 255),
        "buttons": (0, 65535),
    }
    for field, (minimum, maximum) in bounds.items():
        field_value = payload[field]
        if not isinstance(field_value, int) or isinstance(field_value, bool) or not minimum <= field_value <= maximum:
            raise TraceV2Error(f"{name} {field} range")
    return payload


def _derive_receipt_events(
    raw_path: Path,
    role: str,
    receipt: dict[str, Any],
    start_tick: int,
    tick_count: int,
    raw_sample_hz: int,
    projected_frames: bytes | None,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    cadence = receipt["cadence"]
    source_hz = cadence["source_hz"]
    native_hz = cadence["native_hz"]
    hold = cadence["hold"]
    source_markers = receipt["output"]["source_marker_count"]
    frame_count = receipt["output"]["frame_count"]
    if (
        start_tick < 1
        or tick_count <= 0
        or tick_count > MAX_TRACE_TICKS
        or start_tick > source_markers - tick_count + 1
    ):
        raise TraceV2Error("receipt window bounds")

    expected_raw_hz = source_hz if role == "oracle" else native_hz
    raw_tick_count = source_markers if role == "oracle" else frame_count
    if raw_sample_hz != expected_raw_hz:
        raise TraceV2Error("raw capture cadence")
    raw_events = load_jsonl(raw_path, 1, raw_tick_count)

    if role == "native":
        if projected_frames is None or len(projected_frames) != frame_count * 9:
            raise TraceV2Error("projected replay frames")
        for frame_index in range(frame_count):
            event = raw_events[frame_index * len(DOMAINS)]
            observed = _validate_controller_payload(event["payload"], f"raw native frame {frame_index + 1} controller")
            expected = _controller_payload(projected_frames, frame_index * 9)
            if observed != expected:
                raise TraceV2Error(f"raw native frame {frame_index + 1} controller mismatch")
    else:
        if projected_frames is None or len(projected_frames) != frame_count * 9:
            raise TraceV2Error("projected replay frames")
        for marker_index in range(source_markers):
            event = raw_events[marker_index * len(DOMAINS)]
            observed = _validate_controller_payload(event["payload"], f"raw oracle tick {marker_index + 1} controller")
            expected = _controller_payload(projected_frames, marker_index * hold * 9)
            if observed != expected:
                raise TraceV2Error(f"raw oracle tick {marker_index + 1} controller mismatch")

    derived: list[dict[str, Any]] = []
    for logical_tick in range(start_tick, start_tick + tick_count):
        raw_tick = logical_tick if role == "oracle" else logical_tick * hold
        event_offset = (raw_tick - 1) * len(DOMAINS)
        for raw_event in raw_events[event_offset : event_offset + len(DOMAINS)]:
            derived.append(
                {
                    "sequence": len(derived),
                    "tick": logical_tick,
                    "domain": raw_event["domain"],
                    "payload": raw_event["payload"],
                }
            )
    events = validate_events(derived, start_tick, tick_count)
    sampling = {
        "logical_sample_hz": source_hz,
        "oracle": {
            "raw_sample_hz": source_hz,
            "raw_start_tick": 1,
            "raw_tick_count": source_markers,
            "selection": "identity",
            "selected_tick_stride": 1,
            "selected_tick_phase": 1,
        },
        "native": {
            "raw_sample_hz": native_hz,
            "raw_start_tick": 1,
            "raw_tick_count": frame_count,
            "selection": "identity" if hold == 1 else "last_native_tick_in_sample",
            "selected_tick_stride": hold,
            "selected_tick_phase": hold,
        },
    }
    return events, sampling


def _controller_replay_contract(receipt_path: Path, receipt: dict[str, Any], receipt_sha256: str) -> dict[str, Any]:
    source = receipt["source"]
    return {
        "receipt": {"path": receipt_path.as_posix(), "sha256": receipt_sha256},
        "source_replay_sha256": source["raw_replay_sha256"],
        "source_payload_sha256": source["raw_payload_sha256"],
        "projected_replay_sha256": receipt["output"]["output_sha256"],
        "parent_replay_sha256": source["parent_replay_sha256"],
        "parent_payload_sha256": source["parent_payload_sha256"],
        "parent_window": dict(source["parent_window"]),
    }


def build_trace(
    raw_path: Path,
    role: str,
    oracle_commit: str,
    native_commit: str,
    patch_stack: Path,
    binary: Path,
    replay: Path,
    probe: Path,
    probe_id: str,
    window_id: str,
    start_tick: int,
    tick_count: int,
    sample_hz: int,
    projection_receipt: Path | None = None,
) -> dict[str, Any]:
    if role not in {"oracle", "native"}:
        raise TraceV2Error("role")
    if role == "native" and projection_receipt is None:
        raise TraceV2Error("native trace requires a projection receipt")
    if COMMIT.fullmatch(oracle_commit) is None or COMMIT.fullmatch(native_commit) is None:
        raise TraceV2Error("commit identity")
    if not probe_id or not window_id:
        raise TraceV2Error("probe or window identity")
    if (
        not isinstance(start_tick, int)
        or isinstance(start_tick, bool)
        or not isinstance(tick_count, int)
        or isinstance(tick_count, bool)
        or start_tick < 1
        or tick_count <= 0
        or tick_count > MAX_TRACE_TICKS
        or start_tick > MAX_TRACE_TICKS - tick_count + 1
        or sample_hz <= 0
        or NATIVE_SIMULATION_HZ % sample_hz != 0
        or (sample_hz, NATIVE_SIMULATION_HZ, NATIVE_SIMULATION_HZ // sample_hz) not in SUPPORTED_CADENCES
    ):
        raise TraceV2Error("window bounds")
    if projection_receipt is not None:
        receipt, _ = _load_projection_receipt_snapshot(projection_receipt)
        _require_trace_grade_receipt(receipt)

    raw_limit = min(MAX_RAW_BYTES, MAX_TOTAL_INPUT_BYTES)
    raw_data = _read_bounded(
        raw_path,
        raw_limit,
        "raw capture",
        total_limited=raw_limit < MAX_RAW_BYTES,
    )
    total_input_bytes = len(raw_data)
    sealed_artifacts: list[dict[str, str]] = []
    for path, name in (
        (patch_stack, "patch stack"),
        (binary, "binary"),
        (replay, "replay"),
        (probe, "probe"),
    ):
        remaining = MAX_TOTAL_INPUT_BYTES - total_input_bytes
        if remaining <= 0:
            raise TraceV2Error("total input byte bound")
        limit = min(MAX_ARTIFACT_BYTES, remaining)
        sealed, byte_count = _artifact_snapshot(
            path,
            name,
            limit,
            total_limited=limit < MAX_ARTIFACT_BYTES,
        )
        sealed_artifacts.append(sealed)
        total_input_bytes += byte_count
    patch_stack_artifact, binary_artifact, replay_artifact, probe_artifact = sealed_artifacts

    native_ticks_per_sample = NATIVE_SIMULATION_HZ // sample_hz
    cadence = {
        "oracle_update_hz": sample_hz,
        "native_simulation_hz": NATIVE_SIMULATION_HZ,
        "native_ticks_per_sample": native_ticks_per_sample,
        "input_resampling": "identity" if native_ticks_per_sample == 1 else "zero_order_hold",
        "snapshot_sampling": "last_native_tick_in_sample",
    }
    events = load_jsonl_bytes(raw_data, start_tick, tick_count)
    window = {
        "id": window_id,
        "start_tick": start_tick,
        "tick_count": tick_count,
        "sample_hz": sample_hz,
        "cadence": cadence,
        "domains": list(DOMAINS),
    }
    return {
        "schema": TRACE_SCHEMA,
        "header": {
            "role": role,
            "target": {"module": "default.xex", "xex_sha256": XEX_SHA256},
            "commits": {"oracle": oracle_commit, "native": native_commit},
            "patch_stack": patch_stack_artifact,
            "binary": binary_artifact,
            "replay": replay_artifact,
            "probe": {"id": probe_id, **probe_artifact},
            "capture": {
                "path": raw_path.as_posix(),
                "sha256": hashlib.sha256(raw_data).hexdigest(),
            },
            "window": window,
        },
        "event_count": len(events),
        "events": events,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw_jsonl", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--role", choices=("oracle", "native"), required=True)
    parser.add_argument("--oracle-commit", required=True)
    parser.add_argument("--native-commit", required=True)
    parser.add_argument("--patch-stack", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--replay", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--probe-id", required=True)
    parser.add_argument("--window", required=True)
    parser.add_argument("--start-tick", type=int, required=True)
    parser.add_argument("--tick-count", type=int, required=True)
    parser.add_argument("--sample-hz", type=int, default=30)
    parser.add_argument("--projection-receipt", type=Path)
    arguments = parser.parse_args()
    try:
        input_paths = (
            arguments.raw_jsonl,
            arguments.patch_stack,
            arguments.binary,
            arguments.replay,
            arguments.probe,
        )
        if arguments.projection_receipt is not None:
            input_paths += (arguments.projection_receipt,)
        _require_output_not_input(arguments.output, input_paths)
        trace = build_trace(
            arguments.raw_jsonl,
            arguments.role,
            arguments.oracle_commit,
            arguments.native_commit,
            arguments.patch_stack,
            arguments.binary,
            arguments.replay,
            arguments.probe,
            arguments.probe_id,
            arguments.window,
            arguments.start_tick,
            arguments.tick_count,
            arguments.sample_hz,
            arguments.projection_receipt,
        )
        output = (json.dumps(trace, indent=2, sort_keys=True, allow_nan=False) + "\n").encode()
        if len(output) > MAX_TRACE_BYTES:
            raise TraceV2Error("trace output byte bound")
        _atomic_write(arguments.output, output)
    except (OSError, TraceV2Error) as error:
        print(f"execution_trace_v2=fail reason={error}")
        return 1
    print(f"execution_trace_v2=pass role={arguments.role} ticks={arguments.tick_count} events={trace['event_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
