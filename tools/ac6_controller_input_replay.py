#!/usr/bin/env python3
"""Seal, slice, validate, project and compare synchronized AC6 XAM controller polls.

The raw format is shared by the Xenia and AC6_recomp oracle lanes. Its bounded
controller projection targets the native runtime without pretending that the
native runtime has guest XAM calls. It never contains retail executable,
content, profile or save bytes.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import re
import struct
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator, Sequence


SCHEMA = "ac6.controller-input-replay.v1"
PRIMARY_SYNC_KEY = "poll_index"
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")
HEX32 = re.compile(r"[0-9A-F]{8}")
HEX64 = re.compile(r"[0-9a-f]{16}")
XEX_VERSION = re.compile(r"v(?:0|[1-9][0-9]{0,9})(?:\.(?:0|[1-9][0-9]{0,9})){3}")
LANES = {"xenia-canary", "ac6-recomp"}
SEGMENT_ORIGINS = {"clean_boot", "sealed_retail_save"}
SEGMENT_KINDS = {"full_recording", "marker_window"}
MARKER_ROLES = {"ac6_frame_input_stage", "mission_manager_tick"}
MARKER_PHASES = {"before_input", "after_input"}
CADENCE_STATUSES = {"unqualified", "measured"}
RESAMPLING_POLICIES = {"refuse", "identity", "zero_order_hold"}
PROJECTION = "unique_successful_user0_poll"
MAX_REPLAY_BYTES = 128 * 1024 * 1024
MAX_EVENTS = 1_000_000
MAX_POLLS = 1_000_000
MAX_MARKERS = 500_000
MAX_PROJECTED_FRAMES = 1_000_000
MAX_PROJECTED_TSV_BYTES = 128 * 1024 * 1024
MAX_CACHE_INDEX_BYTES = 128 * 1024 * 1024
MAX_RECEIPT_BYTES = 64 * 1024
MAX_PLATFORM_LENGTH = 128
MAX_MODULE_LENGTH = 128
MAX_LINE_BYTES = 1024 * 1024
INPUT_FRAME_BYTES = 9
AC6RTPLY_HEADER_BYTES = 121
MAX_AC6RTPLY_BYTES = AC6RTPLY_HEADER_BYTES + MAX_PROJECTED_FRAMES * INPUT_FRAME_BYTES
AC6RTPLY_MAGIC = b"AC6RTPLY\0"
AC6RTPLY_VERSION = 3
AC6RTPLY_MISSION_ID = 1
AC6RTPLY_NORMAL_DIFFICULTY = 1
AC6RTPLY_AIRCRAFT_ID = 1
AC6RTPLY_WEAPON_ID = 1
AC6RTPLY_RANDOM_SEED = 0xAC60000000000001
CACHE_CURRENT_MAGIC = b"AC6RCUR\0"
CACHE_CURRENT_VERSION = 2
CACHE_CURRENT_SIZE = 48
PROJECTION_RECEIPT_SCHEMA = "ac6.native-controller-projection-receipt.v1"
CONTROLLER_MAPPING = {
    "pitch": "thumb_ly",
    "roll": "thumb_lx",
    "yaw": "left_shoulder=-32768;right_shoulder=32767;otherwise=thumb_rx;left_precedes_right",
    "throttle": "right_trigger",
    "buttons": "raw_xinput_buttons",
}

HEADER_KEYS = {"kind", "schema", "producer", "target", "session", "segment", "sync"}
PRODUCER_KEYS = {"lane", "implementation_commit", "binary_sha256", "platform"}
TARGET_KEYS = {
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
SESSION_KEYS = {
    "content_manifest_sha256",
    "runtime_config_sha256",
    "behavior_config_sha256",
    "profile_save_manifest_sha256",
    "route_sha256",
    "segment_origin",
}
SEGMENT_KEYS = {
    "kind",
    "parent_replay_sha256",
    "parent_payload_sha256",
    "parent_start_marker",
    "parent_marker_count",
}
SYNC_KEYS = {
    "primary",
    "portable_guards",
    "lane_local_diagnostics",
    "telemetry",
    "marker_role",
    "marker_phase",
    "cadence",
}
PORTABLE_GUARDS = (
    "marker_index",
    "poll_in_marker",
    "caller_lr",
    "user_index",
    "flags",
    "state_ptr_null",
)
LANE_LOCAL_DIAGNOSTICS = ("thread_id", "state_ptr")
TELEMETRY = ("guest_tick", "present_index")
CADENCE_KEYS = {"status", "source_hz", "native_hz", "resampling", "projection"}
MARKER_KEYS = {
    "kind",
    "sequence",
    "marker_index",
    "poll_index",
    "guest_tick",
    "present_index",
}
POLL_KEYS = {
    "kind",
    "sequence",
    "poll_index",
    "marker_index",
    "poll_in_marker",
    "guest_tick",
    "present_index",
    "thread_id",
    "caller_lr",
    "user_index",
    "flags",
    "state_ptr",
    "result",
    "state",
}
STATE_KEYS = {
    "packet_number",
    "buttons",
    "left_trigger",
    "right_trigger",
    "thumb_lx",
    "thumb_ly",
    "thumb_rx",
    "thumb_ry",
}
FOOTER_KEYS = {
    "kind",
    "event_count",
    "poll_count",
    "marker_count",
    "present_count",
    "payload_sha256",
}
FOOTER_PAYLOAD_KEYS = FOOTER_KEYS - {"payload_sha256"}


class ReplayError(ValueError):
    pass


@dataclass(frozen=True)
class ReplayDocument:
    header: dict[str, Any]
    events: tuple[dict[str, Any], ...]
    footer: dict[str, Any]


def canonical_line(record: dict[str, Any]) -> bytes:
    return (json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n").encode()


def _sha256_file_bounded(path: Path, maximum_bytes: int, where: str) -> str:
    try:
        size = path.stat().st_size
        if size < 0 or size > maximum_bytes:
            raise ReplayError(f"{where} byte bound")
        digest = hashlib.sha256()
        total = 0
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                total += len(chunk)
                if total > maximum_bytes:
                    raise ReplayError(f"{where} byte bound")
                digest.update(chunk)
        if total != size:
            raise ReplayError(f"{where} changed while hashing")
        return digest.hexdigest()
    except OSError as error:
        raise ReplayError(f"{where} unreadable: {error}") from error


def _prepare_atomic_file(path: Path, data: bytes, maximum_bytes: int) -> Path:
    if not path.name or len(data) > maximum_bytes:
        raise ReplayError("output byte bound")
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            raise ReplayError(f"output already exists: {path}")
        descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
        temporary = Path(temporary_name)
        try:
            with os.fdopen(descriptor, "wb") as destination:
                destination.write(data)
                destination.flush()
                os.fsync(destination.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
        return temporary
    except OSError as error:
        raise ReplayError(f"output preparation failed: {error}") from error


def _publish_atomic_files(files: Sequence[tuple[Path, bytes, int]]) -> None:
    if not files:
        raise ReplayError("no outputs")
    destinations = [path.resolve(strict=False) for path, _, _ in files]
    if len(set(destinations)) != len(destinations):
        raise ReplayError("output paths must be distinct")

    prepared: list[tuple[Path, Path]] = []
    published: list[Path] = []
    try:
        for path, data, maximum in files:
            prepared.append((path, _prepare_atomic_file(path, data, maximum)))
        for destination, temporary in prepared:
            try:
                os.link(temporary, destination)
            except FileExistsError as error:
                raise ReplayError(f"output already exists: {destination}") from error
            published.append(destination)
        for _, temporary in prepared:
            temporary.unlink()
    except BaseException:
        for destination in published:
            destination.unlink(missing_ok=True)
        for _, temporary in prepared:
            temporary.unlink(missing_ok=True)
        raise


def _atomic_write_new(path: Path, data: bytes, maximum_bytes: int) -> None:
    _publish_atomic_files(((path, data, maximum_bytes),))


def require_dict(value: object, keys: set[str], where: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != keys:
        raise ReplayError(f"{where} shape")
    return value


def require_uint(value: object, maximum: int, where: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= maximum:
        raise ReplayError(f"{where} range")
    return value


def require_sint(value: object, minimum: int, maximum: int, where: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or not minimum <= value <= maximum:
        raise ReplayError(f"{where} range")
    return value


def require_sha256(value: object, where: str) -> str:
    if not isinstance(value, str) or SHA256.fullmatch(value) is None:
        raise ReplayError(f"{where} sha256")
    return value


def require_bounded_string(value: object, maximum: int, where: str) -> str:
    if not isinstance(value, str) or not value or len(value) > maximum or "\x00" in value:
        raise ReplayError(f"{where} string")
    return value


def validate_header(header: object) -> dict[str, Any]:
    result = require_dict(header, HEADER_KEYS, "header")
    if result["kind"] != "header" or result["schema"] != SCHEMA:
        raise ReplayError("header identity")

    producer = require_dict(result["producer"], PRODUCER_KEYS, "producer")
    if producer["lane"] not in LANES:
        raise ReplayError("producer lane")
    if not isinstance(producer["implementation_commit"], str) or (
        COMMIT.fullmatch(producer["implementation_commit"]) is None
    ):
        raise ReplayError("producer commit")
    require_sha256(producer["binary_sha256"], "producer binary")
    require_bounded_string(producer["platform"], MAX_PLATFORM_LENGTH, "producer platform")

    target = require_dict(result["target"], TARGET_KEYS, "target")
    if not isinstance(target["title_id"], str) or HEX32.fullmatch(target["title_id"]) is None:
        raise ReplayError("target title_id")
    if not isinstance(target["media_id"], str) or HEX32.fullmatch(target["media_id"]) is None:
        raise ReplayError("target media_id")
    module = require_bounded_string(target["module"], MAX_MODULE_LENGTH, "target module")
    if "/" in module or "\\" in module:
        raise ReplayError("target module basename")
    require_sha256(target["xex_sha256"], "target xex")
    for field in ("xex_version", "base_version"):
        if not isinstance(target[field], str) or XEX_VERSION.fullmatch(target[field]) is None:
            raise ReplayError(f"target {field}")
    if not isinstance(target["module_xxh3"], str) or HEX64.fullmatch(target["module_xxh3"]) is None:
        raise ReplayError("target module_xxh3")
    if not isinstance(target["marker_address"], str) or HEX32.fullmatch(target["marker_address"]) is None:
        raise ReplayError("target marker_address")
    require_sha256(target["marker_code_sha256"], "target marker code")

    session = require_dict(result["session"], SESSION_KEYS, "session")
    for field in (
        "content_manifest_sha256",
        "runtime_config_sha256",
        "behavior_config_sha256",
        "profile_save_manifest_sha256",
        "route_sha256",
    ):
        require_sha256(session[field], f"session {field}")
    if session["segment_origin"] not in SEGMENT_ORIGINS:
        raise ReplayError("session segment_origin")

    segment = require_dict(result["segment"], SEGMENT_KEYS, "segment")
    if segment["kind"] not in SEGMENT_KINDS:
        raise ReplayError("segment kind")
    parent_fields = (
        segment["parent_replay_sha256"],
        segment["parent_payload_sha256"],
        segment["parent_start_marker"],
        segment["parent_marker_count"],
    )
    if segment["kind"] == "full_recording":
        if any(value is not None for value in parent_fields):
            raise ReplayError("full recording lineage")
    else:
        require_sha256(segment["parent_replay_sha256"], "segment parent replay")
        require_sha256(segment["parent_payload_sha256"], "segment parent payload")
        require_uint(segment["parent_start_marker"], MAX_MARKERS, "segment parent_start_marker")
        marker_count = require_uint(segment["parent_marker_count"], MAX_MARKERS, "segment parent_marker_count")
        if segment["parent_start_marker"] == 0 or marker_count == 0:
            raise ReplayError("segment parent marker window")

    sync = require_dict(result["sync"], SYNC_KEYS, "sync")
    if sync["primary"] != PRIMARY_SYNC_KEY:
        raise ReplayError("sync primary")
    if sync["portable_guards"] != list(PORTABLE_GUARDS):
        raise ReplayError("sync portable guards")
    if sync["lane_local_diagnostics"] != list(LANE_LOCAL_DIAGNOSTICS):
        raise ReplayError("sync lane-local diagnostics")
    if sync["telemetry"] != list(TELEMETRY):
        raise ReplayError("sync telemetry")
    if sync["marker_role"] not in MARKER_ROLES:
        raise ReplayError("sync marker role")
    if sync["marker_phase"] not in MARKER_PHASES:
        raise ReplayError("sync marker phase")
    cadence = require_dict(sync["cadence"], CADENCE_KEYS, "sync cadence")
    status = cadence["status"]
    source_hz = cadence["source_hz"]
    native_hz = require_uint(cadence["native_hz"], 1000, "sync cadence native_hz")
    if native_hz == 0 or cadence["projection"] != PROJECTION:
        raise ReplayError("sync cadence projection")
    if status not in CADENCE_STATUSES or cadence["resampling"] not in RESAMPLING_POLICIES:
        raise ReplayError("sync cadence policy")
    if status == "unqualified":
        if source_hz is not None or cadence["resampling"] != "refuse":
            raise ReplayError("sync unqualified cadence")
    else:
        source_hz = require_uint(source_hz, 1000, "sync cadence source_hz")
        if source_hz == 0:
            raise ReplayError("sync cadence source_hz")
        expected_resampling = "identity" if source_hz == native_hz else "zero_order_hold"
        if cadence["resampling"] != expected_resampling or native_hz < source_hz or native_hz % source_hz != 0:
            raise ReplayError("sync measured cadence")
    if segment["kind"] == "full_recording" and status != "unqualified":
        raise ReplayError("full recording cadence must be unqualified")
    if segment["kind"] == "marker_window" and status != "measured":
        raise ReplayError("marker window cadence must be measured")
    return result


def validate_state(state: object, where: str) -> dict[str, int]:
    result = require_dict(state, STATE_KEYS, where)
    require_uint(result["packet_number"], 0xFFFFFFFF, f"{where} packet_number")
    require_uint(result["buttons"], 0xFFFF, f"{where} buttons")
    require_uint(result["left_trigger"], 0xFF, f"{where} left_trigger")
    require_uint(result["right_trigger"], 0xFF, f"{where} right_trigger")
    for field in ("thumb_lx", "thumb_ly", "thumb_rx", "thumb_ry"):
        require_sint(result[field], -32768, 32767, f"{where} {field}")
    return result


def validate_events(events: Sequence[object], marker_phase: str) -> tuple[dict[str, Any], ...]:
    if not events:
        raise ReplayError("empty replay")
    if len(events) > MAX_EVENTS:
        raise ReplayError("event count bound")
    validated: list[dict[str, Any]] = []
    expected_poll = 0
    expected_marker = 1
    current_marker = 0 if marker_phase == "before_input" else 1
    expected_poll_in_marker = 0
    marker_count = 0
    last_present = 0
    last_guest_tick = 0

    for sequence, event_value in enumerate(events):
        if not isinstance(event_value, dict):
            raise ReplayError(f"event {sequence} shape")
        kind = event_value.get("kind")
        if kind == "marker":
            event = require_dict(event_value, MARKER_KEYS, f"event {sequence} marker")
            if event["sequence"] != sequence:
                raise ReplayError(f"event {sequence} sequence")
            if event["marker_index"] != expected_marker:
                raise ReplayError(f"event {sequence} marker_index")
            if event["poll_index"] != expected_poll:
                raise ReplayError(f"event {sequence} marker poll_index")
            if marker_phase == "before_input":
                if marker_count and expected_poll_in_marker == 0:
                    raise ReplayError(f"event {sequence} previous marker has no polls")
            elif expected_poll_in_marker == 0:
                raise ReplayError(f"event {sequence} marker has no polls")
            marker_count += 1
            if marker_phase == "before_input":
                current_marker = expected_marker
            else:
                current_marker = expected_marker + 1
            expected_marker += 1
            expected_poll_in_marker = 0
        elif kind == "poll":
            event = require_dict(event_value, POLL_KEYS, f"event {sequence} poll")
            if event["sequence"] != sequence:
                raise ReplayError(f"event {sequence} sequence")
            if event["poll_index"] != expected_poll:
                raise ReplayError(f"event {sequence} poll_index")
            if event["marker_index"] != current_marker:
                raise ReplayError(f"event {sequence} poll marker_index")
            if event["poll_in_marker"] != expected_poll_in_marker:
                raise ReplayError(f"event {sequence} poll_in_marker")
            require_uint(event["thread_id"], 0xFFFFFFFF, f"event {sequence} thread_id")
            require_uint(event["caller_lr"], 0xFFFFFFFF, f"event {sequence} caller_lr")
            require_uint(event["user_index"], 0xFFFFFFFF, f"event {sequence} user_index")
            require_uint(event["flags"], 0xFFFFFFFF, f"event {sequence} flags")
            state_ptr = require_uint(event["state_ptr"], 0xFFFFFFFF, f"event {sequence} state_ptr")
            result = require_uint(event["result"], 0xFFFFFFFF, f"event {sequence} result")
            if result == 0 and state_ptr:
                validate_state(event["state"], f"event {sequence} state")
            elif event["state"] is not None:
                raise ReplayError(f"event {sequence} state/result/pointer")
            expected_poll += 1
            expected_poll_in_marker += 1
            if expected_poll > MAX_POLLS:
                raise ReplayError("poll count bound")
        else:
            raise ReplayError(f"event {sequence} kind")

        guest_tick = require_uint(event["guest_tick"], 0xFFFFFFFFFFFFFFFF, f"event {sequence} guest_tick")
        present = require_uint(event["present_index"], 0xFFFFFFFFFFFFFFFF, f"event {sequence} present_index")
        if guest_tick < last_guest_tick:
            raise ReplayError(f"event {sequence} guest_tick order")
        if present < last_present:
            raise ReplayError(f"event {sequence} present_index order")
        last_guest_tick = guest_tick
        last_present = present
        validated.append(event)
        if marker_count > MAX_MARKERS:
            raise ReplayError("marker count bound")
    if expected_poll == 0:
        raise ReplayError("replay has no polls")
    if marker_count == 0:
        raise ReplayError("replay has no markers")
    if marker_phase == "before_input" and expected_poll_in_marker == 0:
        raise ReplayError("final marker has no polls")
    if marker_phase == "after_input" and expected_poll_in_marker != 0:
        raise ReplayError("polls after final marker")
    return tuple(validated)


def _validate_segment_marker_count(header: dict[str, Any], marker_count: int) -> None:
    segment = header["segment"]
    if segment["kind"] == "marker_window" and segment["parent_marker_count"] != marker_count:
        raise ReplayError("segment marker_count")


def seal_replay(header: dict[str, Any], events: Sequence[dict[str, Any]], present_count: int | None = None) -> bytes:
    validated_header = validate_header(header)
    validated_events = validate_events(events, validated_header["sync"]["marker_phase"])
    marker_count = sum(event["kind"] == "marker" for event in validated_events)
    poll_count = sum(event["kind"] == "poll" for event in validated_events)
    _validate_segment_marker_count(validated_header, marker_count)
    observed_present = max((event["present_index"] for event in validated_events), default=0)
    if present_count is None:
        present_count = observed_present
    require_uint(present_count, 0xFFFFFFFFFFFFFFFF, "footer present_count")
    if present_count < observed_present:
        raise ReplayError("footer present_count order")

    body_parts = [canonical_line(validated_header)]
    body_size = len(body_parts[0])
    for event in validated_events:
        line = canonical_line(event)
        body_size += len(line)
        if body_size > MAX_REPLAY_BYTES:
            raise ReplayError("sealed file byte bound")
        body_parts.append(line)
    body = b"".join(body_parts)
    footer_payload = {
        "kind": "footer",
        "event_count": len(validated_events),
        "poll_count": poll_count,
        "marker_count": marker_count,
        "present_count": present_count,
    }
    footer = {
        **footer_payload,
        "payload_sha256": hashlib.sha256(body + canonical_line(footer_payload)).hexdigest(),
    }
    data = body + canonical_line(footer)
    if len(data) > MAX_REPLAY_BYTES:
        raise ReplayError("sealed file byte bound")
    return data


def _load_replay_lines(raw_lines: Sequence[bytes], expected_header: dict[str, Any] | None = None) -> ReplayDocument:
    if not raw_lines or not raw_lines[-1].endswith(b"\n") or any(b"\r" in line for line in raw_lines):
        raise ReplayError("file framing")
    if len(raw_lines) > MAX_EVENTS + 2:
        raise ReplayError("file record bound")
    if len(raw_lines) < 2:
        raise ReplayError("file record count")
    records: list[dict[str, Any]] = []
    for line_number, raw_line in enumerate(raw_lines, 1):
        try:
            record = json.loads(raw_line)
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ReplayError(f"line {line_number} JSON") from error
        if not isinstance(record, dict) or canonical_line(record) != raw_line:
            raise ReplayError(f"line {line_number} canonical encoding")
        records.append(record)

    header = validate_header(records[0])
    if expected_header is not None:
        expected = validate_header(expected_header)
        for section in ("producer", "target", "session", "segment", "sync"):
            if header[section] != expected[section]:
                raise ReplayError(f"runtime {section} mismatch")

    footer = require_dict(records[-1], FOOTER_KEYS, "footer")
    if footer["kind"] != "footer":
        raise ReplayError("footer identity")
    events = validate_events(records[1:-1], header["sync"]["marker_phase"])
    marker_count = sum(event["kind"] == "marker" for event in events)
    poll_count = sum(event["kind"] == "poll" for event in events)
    _validate_segment_marker_count(header, marker_count)
    event_count = require_uint(footer["event_count"], MAX_EVENTS, "footer event_count")
    poll_footer_count = require_uint(footer["poll_count"], MAX_POLLS, "footer poll_count")
    marker_footer_count = require_uint(footer["marker_count"], MAX_MARKERS, "footer marker_count")
    if event_count != len(events):
        raise ReplayError("footer event_count")
    if poll_footer_count != poll_count:
        raise ReplayError("footer poll_count")
    if marker_footer_count != marker_count:
        raise ReplayError("footer marker_count")
    observed_present = max((event["present_index"] for event in events), default=0)
    present_count = require_uint(footer["present_count"], 0xFFFFFFFFFFFFFFFF, "footer present_count")
    if present_count < observed_present:
        raise ReplayError("footer present_count order")
    body = b"".join(raw_lines[:-1])
    footer_payload = {key: footer[key] for key in FOOTER_PAYLOAD_KEYS}
    require_sha256(footer["payload_sha256"], "footer payload")
    if footer["payload_sha256"] != hashlib.sha256(body + canonical_line(footer_payload)).hexdigest():
        raise ReplayError("footer payload_sha256")
    return ReplayDocument(header, events, footer)


def load_replay_bytes(data: bytes, expected_header: dict[str, Any] | None = None) -> ReplayDocument:
    if len(data) > MAX_REPLAY_BYTES:
        raise ReplayError("file byte bound")
    raw_lines = data.splitlines(keepends=True)
    if any(len(line) > MAX_LINE_BYTES for line in raw_lines):
        raise ReplayError("file line bound")
    return _load_replay_lines(raw_lines, expected_header)


def load_replay(path: Path, expected_header: dict[str, Any] | None = None) -> ReplayDocument:
    try:
        return _load_replay_lines(_read_bounded_binary_lines(path, MAX_EVENTS + 2), expected_header)
    except OSError as error:
        raise ReplayError(f"replay unreadable: {error}") from error


def _load_replay_file_bytes(path: Path, expected_header: dict[str, Any] | None = None) -> tuple[bytes, ReplayDocument]:
    try:
        raw_lines = _read_bounded_binary_lines(path, MAX_EVENTS + 2)
    except OSError as error:
        raise ReplayError(f"replay unreadable: {error}") from error
    data = b"".join(raw_lines)
    return data, _load_replay_lines(raw_lines, expected_header)


def _require_marker_window(start_marker: int, marker_count: int, where: str) -> tuple[int, int]:
    if (
        not isinstance(start_marker, int)
        or isinstance(start_marker, bool)
        or not isinstance(marker_count, int)
        or isinstance(marker_count, bool)
        or start_marker < 1
        or marker_count <= 0
        or marker_count > MAX_MARKERS
        or start_marker > MAX_MARKERS - marker_count + 1
    ):
        raise ReplayError(f"{where} marker window")
    return start_marker, start_marker + marker_count


def slice_replay(
    parent_replay: bytes,
    start_marker: int,
    marker_count: int,
    source_hz: int,
    native_hz: int,
    expected_header: dict[str, Any] | None = None,
) -> bytes:
    if len(parent_replay) > MAX_REPLAY_BYTES:
        raise ReplayError("parent replay byte bound")
    parent = load_replay_bytes(parent_replay, expected_header)
    if parent.header["segment"]["kind"] != "full_recording":
        raise ReplayError("slice parent must be a full recording")
    parent_replay_sha256 = hashlib.sha256(parent_replay).hexdigest()
    start_marker, end_marker = _require_marker_window(start_marker, marker_count, "slice")
    source_hz = require_uint(source_hz, 1000, "slice source_hz")
    native_hz = require_uint(native_hz, 1000, "slice native_hz")
    if source_hz == 0 or native_hz == 0 or native_hz < source_hz or native_hz % source_hz != 0:
        raise ReplayError("slice cadence")

    marker_ids = {
        event["marker_index"]
        for event in parent.events
        if event["kind"] == "marker" and start_marker <= event["marker_index"] < end_marker
    }
    expected_marker_ids = set(range(start_marker, end_marker))
    if marker_ids != expected_marker_ids:
        missing = min(expected_marker_ids - marker_ids) if expected_marker_ids - marker_ids else start_marker
        raise ReplayError(f"slice marker {missing} is absent")

    selected = [event for event in parent.events if start_marker <= event["marker_index"] < end_marker]
    if not selected:
        raise ReplayError("slice is empty")

    sliced_events: list[dict[str, Any]] = []
    next_poll = 0
    poll_in_marker: dict[int, int] = {}
    for event in selected:
        sliced = copy.deepcopy(event)
        marker = event["marker_index"] - start_marker + 1
        sliced["sequence"] = len(sliced_events)
        sliced["marker_index"] = marker
        if event["kind"] == "poll":
            sliced["poll_index"] = next_poll
            sliced["poll_in_marker"] = poll_in_marker.get(marker, 0)
            poll_in_marker[marker] = sliced["poll_in_marker"] + 1
            next_poll += 1
        else:
            sliced["poll_index"] = next_poll
        sliced_events.append(sliced)

    sliced_header = copy.deepcopy(parent.header)
    sliced_header["segment"] = {
        "kind": "marker_window",
        "parent_replay_sha256": parent_replay_sha256,
        "parent_payload_sha256": parent.footer["payload_sha256"],
        "parent_start_marker": start_marker,
        "parent_marker_count": marker_count,
    }
    sliced_header["sync"]["cadence"] = {
        "status": "measured",
        "source_hz": source_hz,
        "native_hz": native_hz,
        "resampling": "identity" if source_hz == native_hz else "zero_order_hold",
        "projection": PROJECTION,
    }
    return seal_replay(sliced_header, sliced_events)


def poll_events(document: ReplayDocument) -> Iterator[dict[str, Any]]:
    return (event for event in document.events if event["kind"] == "poll")


def _canonical_controller(state: dict[str, int]) -> tuple[int, int, int, int, int]:
    buttons = state["buttons"]
    yaw = -32768 if buttons & 0x0100 else (32767 if buttons & 0x0200 else state["thumb_rx"])
    return state["thumb_ly"], state["thumb_lx"], yaw, state["right_trigger"], buttons


def _projected_controller_frames(
    document: ReplayDocument, start_marker: int, marker_count: int
) -> tuple[list[tuple[int, int, int, int, int]], int]:
    start_marker, end_marker = _require_marker_window(start_marker, marker_count, "projection")
    cadence = document.header["sync"]["cadence"]
    if cadence["status"] != "measured":
        raise ReplayError("controller projection cadence is unqualified")
    source_hz = cadence["source_hz"]
    native_hz = cadence["native_hz"]
    hold = native_hz // source_hz
    if hold == 0 or marker_count > MAX_PROJECTED_FRAMES // hold:
        raise ReplayError("controller projection frame bound")
    if cadence["resampling"] not in {"identity", "zero_order_hold"}:
        raise ReplayError("controller projection resampling")

    marker_ids = {event["marker_index"] for event in document.events if event["kind"] == "marker"}
    selected: dict[int, list[dict[str, int]]] = {}
    for event in poll_events(document):
        marker = event["marker_index"]
        if not start_marker <= marker < end_marker:
            continue
        if event["user_index"] == 0 and event["state_ptr"] != 0 and event["result"] == 0:
            selected.setdefault(marker, []).append(event["state"])

    source_frames: list[tuple[int, int, int, int, int]] = []
    for marker in range(start_marker, end_marker):
        if marker not in marker_ids:
            raise ReplayError(f"marker {marker} is absent")
        states = selected.get(marker, [])
        if len(states) != 1:
            raise ReplayError(f"marker {marker} has {len(states)} successful user-0 states; expected one")
        source_frames.append(_canonical_controller(states[0]))
    return source_frames, hold


def export_controller_tsv(document: ReplayDocument, start_marker: int, marker_count: int) -> str:
    source_frames, hold = _projected_controller_frames(document, start_marker, marker_count)
    rows: list[str] = []
    output_tick = 1
    output_bytes = 0
    for pitch, roll, yaw, throttle, buttons in source_frames:
        for _ in range(hold):
            row = f"{output_tick} {pitch} {roll} {yaw} {throttle} {buttons}\n"
            output_bytes += len(row.encode())
            if output_bytes > MAX_PROJECTED_TSV_BYTES:
                raise ReplayError("controller export byte bound")
            rows.append(row)
            output_tick += 1
    return "".join(rows)


def read_cache_identity(cache: Path) -> str:
    current = cache / "current"
    try:
        if current.stat().st_size != CACHE_CURRENT_SIZE:
            raise ReplayError("cache current record shape")
        with current.open("rb") as source:
            data = source.read(CACHE_CURRENT_SIZE + 1)
    except OSError as error:
        raise ReplayError("cache current record is absent") from error
    if len(data) != CACHE_CURRENT_SIZE or data[:8] != CACHE_CURRENT_MAGIC:
        raise ReplayError("cache current record shape")
    version, size = struct.unpack_from(">II", data, 8)
    digest = data[16:48]
    if version != CACHE_CURRENT_VERSION or size != CACHE_CURRENT_SIZE or not any(digest):
        raise ReplayError("cache current record identity")
    index = cache / "indices" / f"{digest.hex()}.ac6idx"
    if _sha256_file_bounded(index, MAX_CACHE_INDEX_BYTES, "cache index") != digest.hex():
        raise ReplayError("cache index digest mismatch")
    return digest.hex()


def build_ac6rtply_v3(
    raw_replay: bytes,
    cache_index_sha256: str,
    expected_header: dict[str, Any] | None = None,
) -> tuple[bytes, dict[str, Any]]:
    if len(raw_replay) > MAX_REPLAY_BYTES:
        raise ReplayError("projection raw replay byte bound")
    document = load_replay_bytes(raw_replay, expected_header)
    raw_replay_sha256 = hashlib.sha256(raw_replay).hexdigest()
    require_sha256(cache_index_sha256, "projection cache index")
    if not any(bytes.fromhex(cache_index_sha256)):
        raise ReplayError("projection cache index is zero")
    segment = document.header["segment"]
    if segment["kind"] != "marker_window":
        raise ReplayError("AC6RTPLY projection requires a resealed marker window")

    marker_count = document.footer["marker_count"]
    source_frames, hold = _projected_controller_frames(document, 1, marker_count)
    frames = bytearray()
    for pitch, roll, yaw, throttle, buttons in source_frames:
        frame = struct.pack("<hhhBH", pitch, roll, yaw, throttle, buttons)
        frames.extend(frame * hold)
    frame_count = len(source_frames) * hold
    if frame_count == 0 or frame_count > MAX_PROJECTED_FRAMES or len(frames) != frame_count * INPUT_FRAME_BYTES:
        raise ReplayError("AC6RTPLY frame bound")

    input_digest = hashlib.sha256(frames).digest()
    output = bytearray(AC6RTPLY_MAGIC)
    output.extend(
        struct.pack(
            "<IIIIII",
            AC6RTPLY_VERSION,
            AC6RTPLY_MISSION_ID,
            AC6RTPLY_NORMAL_DIFFICULTY,
            AC6RTPLY_AIRCRAFT_ID,
            AC6RTPLY_WEAPON_ID,
            1,
        )
    )
    output.extend(bytes.fromhex(cache_index_sha256))
    output.extend(struct.pack("<QI", AC6RTPLY_RANDOM_SEED, 0))
    output.extend(struct.pack("<Q", frame_count))
    output.extend(input_digest)
    output.extend(struct.pack("<I", frame_count))
    output.extend(frames)
    replay = bytes(output)
    if len(replay) != AC6RTPLY_HEADER_BYTES + frame_count * INPUT_FRAME_BYTES:
        raise ReplayError("AC6RTPLY size")
    if len(replay) > MAX_AC6RTPLY_BYTES:
        raise ReplayError("AC6RTPLY byte bound")

    cadence = document.header["sync"]["cadence"]
    digest_hex = input_digest.hex()
    receipt = {
        "kind": "native_projection_receipt",
        "schema": PROJECTION_RECEIPT_SCHEMA,
        "source": {
            "raw_replay_sha256": raw_replay_sha256,
            "raw_payload_sha256": document.footer["payload_sha256"],
            "parent_replay_sha256": segment["parent_replay_sha256"],
            "parent_payload_sha256": segment["parent_payload_sha256"],
            "parent_window": {
                "start_marker": segment["parent_start_marker"],
                "marker_count": segment["parent_marker_count"],
            },
        },
        "target": copy.deepcopy(document.header["target"]),
        "cadence": {
            "source_hz": cadence["source_hz"],
            "native_hz": cadence["native_hz"],
            "resampling": cadence["resampling"],
            "hold": hold,
        },
        "mapping": copy.deepcopy(CONTROLLER_MAPPING),
        "cache_index_sha256": cache_index_sha256,
        "output": {
            "format": "AC6RTPLY",
            "version": AC6RTPLY_VERSION,
            "mission_id": AC6RTPLY_MISSION_ID,
            "difficulty": AC6RTPLY_NORMAL_DIFFICULTY,
            "difficulty_name": "Normal",
            "aircraft_id": AC6RTPLY_AIRCRAFT_ID,
            "weapon_id": AC6RTPLY_WEAPON_ID,
            "capability_data_valid": True,
            "random_seed": AC6RTPLY_RANDOM_SEED,
            "checkpoint_count": 0,
            "source_marker_count": marker_count,
            "frame_count": frame_count,
            "final_tick": frame_count,
            "input_digest_sha256": digest_hex,
            "final_digest_sha256": digest_hex,
            "output_sha256": hashlib.sha256(replay).hexdigest(),
        },
    }
    receipt_bytes = canonical_line(receipt)
    if len(receipt_bytes) > MAX_RECEIPT_BYTES:
        raise ReplayError("projection receipt byte bound")
    return replay, receipt


def compare_runs(recorded: ReplayDocument, replayed: ReplayDocument) -> dict[str, Any]:
    recorded_producer = recorded.header["producer"]
    replayed_producer = replayed.header["producer"]
    same_lane = recorded_producer["lane"] == replayed_producer["lane"]
    comparison_policy = "same_lane_strict" if same_lane else "cross_lane_portable"
    if same_lane and recorded_producer != replayed_producer:
        raise ReplayError("comparison producer mismatch")
    for section in ("target", "sync"):
        if recorded.header[section] != replayed.header[section]:
            raise ReplayError(f"comparison {section} mismatch")
    for field in ("kind", "parent_start_marker", "parent_marker_count"):
        if recorded.header["segment"][field] != replayed.header["segment"][field]:
            raise ReplayError(f"comparison segment {field} mismatch")
    if same_lane:
        if recorded.header["session"] != replayed.header["session"]:
            raise ReplayError("comparison session mismatch")
    else:
        portable_session_fields = (
            "content_manifest_sha256",
            "behavior_config_sha256",
            "profile_save_manifest_sha256",
            "route_sha256",
            "segment_origin",
        )
        for field in portable_session_fields:
            if recorded.header["session"][field] != replayed.header["session"][field]:
                raise ReplayError(f"comparison session {field} mismatch")
    expected = list(recorded.events)
    actual = list(replayed.events)
    telemetry = {
        "max_guest_tick_delta": max(
            (abs(reference["guest_tick"] - candidate["guest_tick"]) for reference, candidate in zip(expected, actual)),
            default=0,
        ),
        "max_present_index_delta": max(
            (
                abs(reference["present_index"] - candidate["present_index"])
                for reference, candidate in zip(expected, actual)
            ),
            default=0,
        ),
    }
    if len(expected) != len(actual):
        return {
            "equal": False,
            "comparison_policy": comparison_policy,
            "polls_compared": min(
                sum(event["kind"] == "poll" for event in expected),
                sum(event["kind"] == "poll" for event in actual),
            ),
            "first_divergence": {"field": "event_count", "recorded": len(expected), "replayed": len(actual)},
            "telemetry": telemetry,
        }
    poll_strict_fields = (
        "poll_index",
        "marker_index",
        "poll_in_marker",
        "caller_lr",
        "user_index",
        "flags",
        "result",
        "state",
    )
    marker_strict_fields = ("marker_index", "poll_index")
    max_guest_tick_delta = 0
    max_present_delta = 0
    polls_compared = 0
    for index, (reference, candidate) in enumerate(zip(expected, actual)):
        if reference["kind"] != candidate["kind"]:
            return {
                "equal": False,
                "comparison_policy": comparison_policy,
                "polls_compared": polls_compared,
                "first_divergence": {
                    "sequence": index,
                    "field": "kind",
                    "recorded": reference["kind"],
                    "replayed": candidate["kind"],
                },
                "telemetry": {
                    "max_guest_tick_delta": max_guest_tick_delta,
                    "max_present_index_delta": max_present_delta,
                },
            }
        strict_fields = poll_strict_fields if reference["kind"] == "poll" else marker_strict_fields
        for field in strict_fields:
            if reference[field] != candidate[field]:
                return {
                    "equal": False,
                    "comparison_policy": comparison_policy,
                    "polls_compared": polls_compared,
                    "first_divergence": {
                        "sequence": index,
                        "field": field,
                        "recorded": reference[field],
                        "replayed": candidate[field],
                    },
                    "telemetry": {
                        "max_guest_tick_delta": max_guest_tick_delta,
                        "max_present_index_delta": max_present_delta,
                    },
                }
        if reference["kind"] == "poll":
            if bool(reference["state_ptr"]) != bool(candidate["state_ptr"]):
                return {
                    "equal": False,
                    "comparison_policy": comparison_policy,
                    "polls_compared": polls_compared,
                    "first_divergence": {
                        "sequence": index,
                        "field": "state_ptr_null",
                        "recorded": reference["state_ptr"] == 0,
                        "replayed": candidate["state_ptr"] == 0,
                    },
                    "telemetry": telemetry,
                }
            if same_lane:
                for field in LANE_LOCAL_DIAGNOSTICS:
                    if reference[field] != candidate[field]:
                        return {
                            "equal": False,
                            "comparison_policy": comparison_policy,
                            "polls_compared": polls_compared,
                            "first_divergence": {
                                "sequence": index,
                                "field": field,
                                "recorded": reference[field],
                                "replayed": candidate[field],
                            },
                            "telemetry": telemetry,
                        }
        if reference["kind"] == "poll":
            polls_compared += 1
        max_guest_tick_delta = max(max_guest_tick_delta, abs(reference["guest_tick"] - candidate["guest_tick"]))
        max_present_delta = max(max_present_delta, abs(reference["present_index"] - candidate["present_index"]))
    return {
        "equal": True,
        "comparison_policy": comparison_policy,
        "polls_compared": polls_compared,
        "first_divergence": None,
        "telemetry": {
            "max_guest_tick_delta": max_guest_tick_delta,
            "max_present_index_delta": max_present_delta,
        },
    }


def _read_json(path: Path) -> dict[str, Any]:
    try:
        lines = _read_bounded_binary_lines(path, MAX_EVENTS + 2)
        value = json.loads(b"".join(lines))
    except (OSError, json.JSONDecodeError) as error:
        raise ReplayError(f"JSON unreadable: {path}") from error
    if not isinstance(value, dict):
        raise ReplayError(f"JSON object required: {path}")
    return value


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    values: list[dict[str, Any]] = []
    try:
        for line_number, raw_line in enumerate(_read_bounded_binary_lines(path, MAX_EVENTS), 1):
            if not raw_line.strip():
                continue
            value = json.loads(raw_line)
            if not isinstance(value, dict):
                raise ReplayError(f"event line {line_number} shape")
            values.append(value)
    except (OSError, json.JSONDecodeError) as error:
        raise ReplayError(f"events unreadable: {path}") from error
    return values


def _read_bounded_binary_lines(path: Path, maximum_records: int) -> list[bytes]:
    if path.stat().st_size > MAX_REPLAY_BYTES:
        raise ReplayError(f"input exceeds byte bound: {path}")
    lines: list[bytes] = []
    total = 0
    with path.open("rb") as source:
        for line_number, raw_line in enumerate(source, 1):
            total += len(raw_line)
            if total > MAX_REPLAY_BYTES:
                raise ReplayError(f"input exceeds byte bound: {path}")
            if len(raw_line) > MAX_LINE_BYTES:
                raise ReplayError(f"input line {line_number} exceeds byte bound")
            if line_number > maximum_records:
                raise ReplayError(f"input exceeds record bound: {path}")
            lines.append(raw_line)
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    seal_parser = subparsers.add_parser("seal")
    seal_parser.add_argument("header", type=Path)
    seal_parser.add_argument("events", type=Path)
    seal_parser.add_argument("output", type=Path)
    seal_parser.add_argument("--present-count", type=int)

    slice_parser = subparsers.add_parser("slice-reseal")
    slice_parser.add_argument("replay", type=Path)
    slice_parser.add_argument("output", type=Path)
    slice_parser.add_argument("--expected-header", type=Path, required=True)
    slice_parser.add_argument("--start-marker", type=int, required=True)
    slice_parser.add_argument("--marker-count", type=int, required=True)
    slice_parser.add_argument("--source-hz", type=int, required=True)
    slice_parser.add_argument("--native-hz", type=int, default=60)

    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("replay", type=Path)
    verify_parser.add_argument("--expected-header", type=Path)

    export_parser = subparsers.add_parser("export-controller-tsv")
    export_parser.add_argument("replay", type=Path)
    export_parser.add_argument("output", type=Path)
    export_parser.add_argument("--expected-header", type=Path, required=True)
    export_parser.add_argument("--start-marker", type=int, default=1)
    export_parser.add_argument("--marker-count", type=int, required=True)

    project_parser = subparsers.add_parser("project-ac6rtply-v3")
    project_parser.add_argument("replay", type=Path)
    project_parser.add_argument("cache", type=Path)
    project_parser.add_argument("output", type=Path)
    project_parser.add_argument("receipt", type=Path)
    project_parser.add_argument("--expected-header", type=Path, required=True)

    compare_parser = subparsers.add_parser("compare")
    compare_parser.add_argument("recorded", type=Path)
    compare_parser.add_argument("replayed", type=Path)
    compare_parser.add_argument("--expected-recorded-header", type=Path, required=True)
    compare_parser.add_argument("--expected-replayed-header", type=Path, required=True)

    arguments = parser.parse_args()
    try:
        if arguments.command == "seal":
            data = seal_replay(
                _read_json(arguments.header),
                _read_jsonl(arguments.events),
                arguments.present_count,
            )
            _atomic_write_new(arguments.output, data, MAX_REPLAY_BYTES)
            print(f"controller_replay=sealed sha256={hashlib.sha256(data).hexdigest()}")
        elif arguments.command == "slice-reseal":
            expected_header = _read_json(arguments.expected_header)
            parent_data, _ = _load_replay_file_bytes(arguments.replay, expected_header)
            parent_sha256 = hashlib.sha256(parent_data).hexdigest()
            data = slice_replay(
                parent_data,
                arguments.start_marker,
                arguments.marker_count,
                arguments.source_hz,
                arguments.native_hz,
                expected_header,
            )
            _atomic_write_new(arguments.output, data, MAX_REPLAY_BYTES)
            print(
                "controller_replay=sliced "
                f"markers={arguments.marker_count} parent_sha256={parent_sha256} "
                f"sha256={hashlib.sha256(data).hexdigest()}"
            )
        elif arguments.command == "verify":
            expected = _read_json(arguments.expected_header) if arguments.expected_header else None
            document = load_replay(arguments.replay, expected)
            qualification = "identity_checked" if expected is not None else "structural_only"
            print(
                "controller_replay=valid "
                f"qualification={qualification} "
                f"events={document.footer['event_count']} polls={document.footer['poll_count']} "
                f"markers={document.footer['marker_count']}"
            )
        elif arguments.command == "export-controller-tsv":
            document = load_replay(arguments.replay, _read_json(arguments.expected_header))
            output = export_controller_tsv(document, arguments.start_marker, arguments.marker_count)
            output_bytes = output.encode()
            _atomic_write_new(arguments.output, output_bytes, MAX_PROJECTED_TSV_BYTES)
            print(
                "controller_replay=exported "
                f"rows={len(output.splitlines())} "
                f"sha256={hashlib.sha256(output_bytes).hexdigest()}"
            )
        elif arguments.command == "project-ac6rtply-v3":
            expected_header = _read_json(arguments.expected_header)
            raw_data, _ = _load_replay_file_bytes(arguments.replay, expected_header)
            cache_index_sha256 = read_cache_identity(arguments.cache)
            output, receipt = build_ac6rtply_v3(raw_data, cache_index_sha256, expected_header)
            receipt_bytes = canonical_line(receipt)
            _publish_atomic_files(
                (
                    (arguments.output, output, MAX_AC6RTPLY_BYTES),
                    (arguments.receipt, receipt_bytes, MAX_RECEIPT_BYTES),
                )
            )
            print(
                "controller_replay=projected format=AC6RTPLY version=3 "
                f"frames={receipt['output']['frame_count']} "
                f"input_sha256={receipt['output']['input_digest_sha256']} "
                f"output_sha256={receipt['output']['output_sha256']}"
            )
        else:
            recorded = load_replay(arguments.recorded, _read_json(arguments.expected_recorded_header))
            replayed = load_replay(arguments.replayed, _read_json(arguments.expected_replayed_header))
            report = compare_runs(recorded, replayed)
            print(json.dumps(report, sort_keys=True))
            if not report["equal"]:
                return 1
    except (OSError, ReplayError) as error:
        print(f"controller_replay=fail reason={error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
