#!/usr/bin/env python3
"""Seal ordered AC6 Mission 01 JSONL events as ac6.execution-trace.v2."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
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
MAX_LINE_BYTES = 1024 * 1024
MAX_RECEIPT_BYTES = 64 * 1024
PROJECTION_RECEIPT_SCHEMA = "ac6.native-controller-projection-receipt.v1"
SUPPORTED_CADENCES = {(30, 60, 2), (60, 60, 1)}
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


class TraceV2Error(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_file(path: Path, name: str) -> None:
    if not path.is_file():
        raise TraceV2Error(f"{name} is not a file")


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


def _read_bounded(path: Path, maximum_bytes: int, name: str) -> bytes:
    require_file(path, name)
    size = path.stat().st_size
    if size < 0 or size > maximum_bytes:
        raise TraceV2Error(f"{name} byte bound")
    data = path.read_bytes()
    if len(data) != size:
        raise TraceV2Error(f"{name} changed while reading")
    return data


def validate_events(events: object, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    if not isinstance(events, list) or len(events) != tick_count * len(DOMAINS):
        raise TraceV2Error("event count")
    for sequence, event in enumerate(events):
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
        if (not isinstance(event["tick"], int) or isinstance(event["tick"], bool) or
                event["tick"] != expected_tick):
            raise TraceV2Error(f"event {sequence} tick")
        if event["domain"] != expected_domain:
            raise TraceV2Error(f"event {sequence} domain")
        if not isinstance(event["payload"], dict):
            raise TraceV2Error(f"event {sequence} payload")
        if expected_domain == "output_hashes":
            if not event["payload"] or any(
                not isinstance(name, str) or not name or
                not isinstance(value, str) or SHA256.fullmatch(value) is None
                for name, value in event["payload"].items()
            ):
                raise TraceV2Error(f"event {sequence} output hashes")
    return events


def load_jsonl(path: Path, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    if tick_count <= 0 or tick_count > MAX_PROJECTED_FRAMES:
        raise TraceV2Error("raw tick count bound")
    require_file(path, "raw capture")
    if path.stat().st_size > MAX_RAW_BYTES:
        raise TraceV2Error("raw capture byte bound")
    maximum_events = tick_count * len(DOMAINS)
    events: list[dict[str, Any]] = []
    with path.open("rb") as source:
        for line_number, raw_line in enumerate(source, 1):
            if len(raw_line) > MAX_LINE_BYTES:
                raise TraceV2Error(f"line {line_number}: byte bound")
            if not raw_line.strip():
                continue
            if len(events) >= maximum_events:
                raise TraceV2Error("event count")
            try:
                events.append(json.loads(raw_line))
            except UnicodeDecodeError as error:
                raise TraceV2Error(f"line {line_number}: utf-8") from error
            except json.JSONDecodeError as error:
                raise TraceV2Error(f"line {line_number}: {error.msg}") from error
    return validate_events(events, start_tick, tick_count)


def _validate_projection_receipt(document: object) -> dict[str, Any]:
    receipt = require_dict(document, RECEIPT_KEYS, "projection receipt")
    if (
        receipt["kind"] != "native_projection_receipt"
        or receipt["schema"] != PROJECTION_RECEIPT_SCHEMA
    ):
        raise TraceV2Error("projection receipt identity")

    source = require_dict(receipt["source"], RECEIPT_SOURCE_KEYS, "projection source")
    for key in (
        "raw_replay_sha256",
        "raw_payload_sha256",
        "parent_replay_sha256",
        "parent_payload_sha256",
    ):
        require_sha256(source[key], f"projection source {key}")
    parent_window = require_dict(
        source["parent_window"], {"start_marker", "marker_count"}, "projection parent window"
    )
    parent_start = require_uint(parent_window["start_marker"], MAX_SOURCE_MARKERS, "projection parent start")
    parent_count = require_uint(
        parent_window["marker_count"], MAX_SOURCE_MARKERS, "projection parent count"
    )
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
    for key in ("xex_version", "base_version"):
        if not isinstance(target[key], str) or XEX_VERSION.fullmatch(target[key]) is None:
            raise TraceV2Error(f"projection target {key}")
    require_sha256(target["marker_code_sha256"], "projection target marker code")

    cadence = require_dict(
        receipt["cadence"], {"source_hz", "native_hz", "resampling", "hold"}, "projection cadence"
    )
    source_hz = require_uint(cadence["source_hz"], 1000, "projection source_hz")
    native_hz = require_uint(cadence["native_hz"], 1000, "projection native_hz")
    hold = require_uint(cadence["hold"], 1000, "projection hold")
    if (source_hz, native_hz, hold) not in SUPPORTED_CADENCES:
        raise TraceV2Error("projection cadence unsupported")
    expected_resampling = "identity" if hold == 1 else "zero_order_hold"
    if cadence["resampling"] != expected_resampling:
        raise TraceV2Error("projection cadence resampling")
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
    source_markers = require_uint(
        output["source_marker_count"], MAX_SOURCE_MARKERS, "projection source markers"
    )
    frame_count = require_uint(output["frame_count"], MAX_PROJECTED_FRAMES, "projection frame count")
    final_tick = require_uint(output["final_tick"], MAX_PROJECTED_FRAMES, "projection final tick")
    if (
        random_seed == 0
        or source_markers == 0
        or source_markers != parent_count
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


def load_projection_receipt(path: Path) -> dict[str, Any]:
    data = _read_bounded(path, MAX_RECEIPT_BYTES, "projection receipt")
    try:
        receipt = _validate_projection_receipt(json.loads(data))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise TraceV2Error("projection receipt JSON") from error
    canonical = (json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n").encode()
    if data != canonical:
        raise TraceV2Error("projection receipt is not canonical")
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
    native_hz = require_uint(
        cadence["native_simulation_hz"], 1000, f"{name} native hz"
    )
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
    parent = require_dict(
        contract["parent_window"], {"start_marker", "marker_count"}, f"{name} parent window"
    )
    parent_start = require_uint(parent["start_marker"], MAX_SOURCE_MARKERS, f"{name} parent start")
    parent_count = require_uint(parent["marker_count"], MAX_SOURCE_MARKERS, f"{name} parent count")
    if (
        parent_start == 0
        or parent_count == 0
        or parent_start > MAX_SOURCE_MARKERS - parent_count + 1
    ):
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
    return {"path": path.as_posix(), "sha256": sha256(path)}


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
        if (
            not isinstance(field_value, int)
            or isinstance(field_value, bool)
            or not minimum <= field_value <= maximum
        ):
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
            observed = _validate_controller_payload(
                event["payload"], f"raw native frame {frame_index + 1} controller"
            )
            expected = _controller_payload(projected_frames, frame_index * 9)
            if observed != expected:
                raise TraceV2Error(f"raw native frame {frame_index + 1} controller mismatch")
    else:
        if projected_frames is None or len(projected_frames) != frame_count * 9:
            raise TraceV2Error("projected replay frames")
        for marker_index in range(source_markers):
            event = raw_events[marker_index * len(DOMAINS)]
            observed = _validate_controller_payload(
                event["payload"], f"raw oracle tick {marker_index + 1} controller"
            )
            expected = _controller_payload(projected_frames, marker_index * hold * 9)
            if observed != expected:
                raise TraceV2Error(f"raw oracle tick {marker_index + 1} controller mismatch")

    derived: list[dict[str, Any]] = []
    for logical_tick in range(start_tick, start_tick + tick_count):
        raw_tick = logical_tick if role == "oracle" else logical_tick * hold
        event_offset = (raw_tick - 1) * len(DOMAINS)
        for raw_event in raw_events[event_offset:event_offset + len(DOMAINS)]:
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


def _controller_replay_contract(
    receipt_path: Path, receipt: dict[str, Any]
) -> dict[str, Any]:
    source = receipt["source"]
    return {
        "receipt": artifact(receipt_path),
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
    native_simulation_hz: int,
    native_ticks_per_sample: int,
    projection_receipt: Path | None = None,
) -> dict[str, Any]:
    if role not in {"oracle", "native"}:
        raise TraceV2Error("role")
    if COMMIT.fullmatch(oracle_commit) is None or COMMIT.fullmatch(native_commit) is None:
        raise TraceV2Error("commit identity")
    if not probe_id or not window_id:
        raise TraceV2Error("probe or window identity")
    if (
        not isinstance(start_tick, int)
        or isinstance(start_tick, bool)
        or not isinstance(tick_count, int)
        or isinstance(tick_count, bool)
        or start_tick < 0
        or tick_count <= 0
        or tick_count > MAX_TRACE_TICKS
        or (sample_hz, native_simulation_hz, native_ticks_per_sample) not in SUPPORTED_CADENCES
    ):
        raise TraceV2Error("window bounds")
    for path, name in (
        (raw_path, "raw capture"),
        (patch_stack, "patch stack"),
        (binary, "binary"),
        (replay, "replay"),
        (probe, "probe"),
    ):
        require_file(path, name)
    cadence = {
        "oracle_update_hz": sample_hz,
        "native_simulation_hz": native_simulation_hz,
        "native_ticks_per_sample": native_ticks_per_sample,
        "input_resampling": "identity" if native_ticks_per_sample == 1 else "zero_order_hold",
        "snapshot_sampling": "last_native_tick_in_sample",
    }
    controller_replay = None
    observation = None
    if projection_receipt is None:
        events = load_jsonl(raw_path, start_tick, tick_count)
    else:
        receipt = load_projection_receipt(projection_receipt)
        receipt_cadence = receipt["cadence"]
        if (
            sample_hz,
            native_simulation_hz,
            native_ticks_per_sample,
        ) != (
            receipt_cadence["source_hz"],
            receipt_cadence["native_hz"],
            receipt_cadence["hold"],
        ):
            raise TraceV2Error("arguments disagree with projection receipt cadence")
        if artifact(replay)["sha256"] != receipt["output"]["output_sha256"]:
            raise TraceV2Error("replay disagrees with projection receipt")
        projected_frames = _projected_replay_frames(replay, receipt)
        events, observation = _derive_receipt_events(
            raw_path,
            role,
            receipt,
            start_tick,
            tick_count,
            sample_hz if role == "oracle" else native_simulation_hz,
            projected_frames,
        )
        controller_replay = _controller_replay_contract(projection_receipt, receipt)
        receipt_target = receipt["target"]
        if receipt_target["module"] != "default.xex" or receipt_target["xex_sha256"] != XEX_SHA256:
            raise TraceV2Error("receipt target disagrees with trace target")
    window = {
        "id": window_id,
        "start_tick": start_tick,
        "tick_count": tick_count,
        "sample_hz": sample_hz,
        "cadence": cadence,
        "domains": list(DOMAINS),
    }
    if controller_replay is not None and observation is not None:
        window["controller_replay"] = controller_replay
        window["observation"] = observation
    return {
        "schema": TRACE_SCHEMA,
        "header": {
            "role": role,
            "target": {"module": "default.xex", "xex_sha256": XEX_SHA256},
            "commits": {"oracle": oracle_commit, "native": native_commit},
            "patch_stack": artifact(patch_stack),
            "binary": artifact(binary),
            "replay": artifact(replay),
            "probe": {"id": probe_id, **artifact(probe)},
            "capture": artifact(raw_path),
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
    parser.add_argument("--native-simulation-hz", type=int, default=60)
    parser.add_argument("--native-ticks-per-sample", type=int, default=2)
    parser.add_argument("--projection-receipt", type=Path)
    arguments = parser.parse_args()
    try:
        trace = build_trace(
            arguments.raw_jsonl, arguments.role, arguments.oracle_commit,
            arguments.native_commit, arguments.patch_stack, arguments.binary,
            arguments.replay, arguments.probe, arguments.probe_id,
            arguments.window, arguments.start_tick, arguments.tick_count,
            arguments.sample_hz, arguments.native_simulation_hz,
            arguments.native_ticks_per_sample, arguments.projection_receipt,
        )
        arguments.output.write_text(
            json.dumps(trace, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, TraceV2Error) as error:
        print(f"execution_trace_v2=fail reason={error}")
        return 1
    print(f"execution_trace_v2=pass role={arguments.role} "
          f"ticks={arguments.tick_count} events={trace['event_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
