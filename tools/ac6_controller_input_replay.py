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
import math
import os
import re
import stat
import struct
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator, Sequence


SCHEMA_V3 = "ac6.controller-input-replay.v3"
SCHEMA_V4 = "ac6.controller-input-replay.v4"
SCHEMA = SCHEMA_V3
CADENCE_CENSUS_SCHEMA_V1 = "ac6.controller-cadence-census.v1"
CADENCE_CENSUS_SCHEMA_V2 = "ac6.controller-cadence-census.v2"
CADENCE_CENSUS_SCHEMA = CADENCE_CENSUS_SCHEMA_V1
REFERENCE_CLOCK_SCHEMA = "ac6.fixed-rate-reference-clock.v1"
NATIVE_CLOCK_SCHEMA = "ac6.native-simulation-clock.v1"
NATIVE_CLOCK_CONTRACT = {
    "schema": NATIVE_CLOCK_SCHEMA,
    "clock_id": "ac6_native_fixed_step",
    "frequency": {"numerator": 60, "denominator": 1},
    "tick_semantics": "one_simulation_step",
}
SUPPORTED_CADENCES = {(30, 60, 2), (60, 60, 1)}
PAL_TARGET_IDENTITY = {
    "title_id": "4E4D07D1",
    "media_id": "0379EFB3",
    "module": "default.xex",
    "xex_sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
    "xex_version": "v0.0.0.11",
    "base_version": "v0.0.0.11",
}
PAL_NATIVE_TARGET_IDENTITY = {
    "target_id": "ac6-pal-default-xex",
    "title_id": "4E4D07D1",
    "media_id": "0379EFB3",
    "module": "default.xex",
    "xex_sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
    "xex_version": "v0.0.0.11",
    "base_version": "v0.0.0.11",
}
NTSC_UJ_ORACLE_TARGET_IDENTITY = {
    "target_id": "ac6-ntsc-uj-default-xex",
    "title_id": "4E4D07D1",
    "media_id": "531C30BE",
    "module": "default.xex",
    "xex_sha256": "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc",
    "xex_version": "v0.0.0.8",
    "base_version": "v0.0.0.8",
    "module_xxh3": "892639B654015428",
    "entry_point": "821F5ED0",
    "region_mask": "0000FDFF",
}
NTSC_UJ_MARKER_CONTRACT = {
    "role": "ac6_frame_input_stage",
    "address": "821CA940",
    "phase": "before_input",
    "code": {
        "image_rva": "001CA940",
        "length": 328,
        "sha256": "a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d",
    },
}
PRIMARY_SYNC_KEY = "poll_index"
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")
HEX32 = re.compile(r"[0-9A-F]{8}")
HEX64 = re.compile(r"[0-9a-f]{16}")
HEX64_UPPER = re.compile(r"[0-9A-F]{16}")
XEX_VERSION = re.compile(r"v(?:0|[1-9][0-9]{0,9})(?:\.(?:0|[1-9][0-9]{0,9})){3}")
LANES = {"xenia-canary", "ac6-recomp"}
SEGMENT_ORIGINS = {"clean_boot", "sealed_retail_save"}
SEGMENT_KINDS = {"full_recording", "marker_window"}
MARKER_ROLES = {"ac6_frame_input_stage", "mission_manager_tick"}
MARKER_PHASES = {"before_input", "after_input"}
CADENCE_STATUSES = {"unqualified", "derived"}
CADENCE_INTEGRITY_LEVEL = "integrity_only_runtime_census"
CADENCE_METHOD = "uniform_marker_interval_v1"
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
MAX_CADENCE_CENSUS_BYTES = 16 * 1024 * 1024
MAX_CADENCE_RECORDS = MAX_MARKERS
MAX_MARKER_CODE_BYTES = 4096
MAX_PLATFORM_LENGTH = 128
MAX_MODULE_LENGTH = 128
MAX_LINE_BYTES = 1024 * 1024
MAX_JSON_DEPTH = 64
MAX_JSON_INTEGER_DIGITS = 20
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
PROJECTION_RECEIPT_SCHEMA_V3 = "ac6.native-controller-projection-receipt.v3"
PROJECTION_RECEIPT_SCHEMA_V4 = "ac6.native-controller-projection-receipt.v4"
PROJECTION_RECEIPT_SCHEMA = PROJECTION_RECEIPT_SCHEMA_V3
CONTROLLER_MAPPING = {
    "pitch": "thumb_ly",
    "roll": "thumb_lx",
    "yaw": "left_shoulder=-32768;right_shoulder=32767;otherwise=thumb_rx;left_precedes_right",
    "throttle": "right_trigger",
    "buttons": "raw_xinput_buttons",
}

HEADER_KEYS = {"kind", "schema", "producer", "target", "session", "segment", "sync"}
PRODUCER_KEYS = {"lane", "implementation_commit", "binary_sha256", "build_sha256", "platform"}
TARGET_KEYS = {
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
TARGET_V4_KEYS = set(NTSC_UJ_ORACLE_TARGET_IDENTITY)
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
SYNC_V4_KEYS = (SYNC_KEYS - {"marker_role", "marker_phase"}) | {"marker_contract"}
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
CADENCE_KEYS = {
    "status",
    "integrity_level",
    "source_hz",
    "native_hz",
    "resampling",
    "projection",
    "census",
    "native_clock",
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
CENSUS_KEYS = {
    "kind",
    "schema",
    "integrity_level",
    "producer",
    "configuration",
    "parent",
    "target",
    "marker_contract",
    "clock",
    "records",
    "payload_sha256",
}
CENSUS_BODY_KEYS = CENSUS_KEYS - {"payload_sha256"}
CENSUS_CONFIGURATION_KEYS = {"runtime_config_sha256", "behavior_config_sha256"}
CENSUS_PARENT_KEYS = {"replay_sha256", "payload_sha256", "window"}
CENSUS_WINDOW_KEYS = {"start_marker", "marker_count"}
MARKER_CONTRACT_KEYS = {"role", "address", "phase", "code"}
MARKER_CODE_KEYS = {"offset", "length", "sha256"}
MARKER_CODE_V4_KEYS = {"image_rva", "length", "sha256"}
REFERENCE_CLOCK_KEYS = {"schema", "clock_id", "frequency", "counter_bits", "read_semantics"}
NATIVE_CLOCK_KEYS = {"schema", "clock_id", "frequency", "tick_semantics"}
CENSUS_RECORD_KEYS = {"sequence", "parent_marker_index", "event_sequence", "reference_tick"}
RATIONAL_KEYS = {"numerator", "denominator"}
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


def _parse_json_int(value: str) -> int:
    if len(value.removeprefix("-")) > MAX_JSON_INTEGER_DIGITS:
        raise ReplayError("JSON integer bound")
    return int(value)


def _reject_json_constant(value: str) -> None:
    raise ReplayError(f"non-finite JSON number: {value}")


def _strict_json_loads(data: bytes, where: str) -> object:
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
            if depth > MAX_JSON_DEPTH:
                raise ReplayError(f"{where} JSON depth bound")
        elif byte in (0x5D, 0x7D):
            depth -= 1
            if depth < 0:
                raise ReplayError(f"{where} JSON nesting")
    if in_string or depth != 0:
        raise ReplayError(f"{where} JSON nesting")
    try:
        return json.loads(data, parse_int=_parse_json_int, parse_constant=_reject_json_constant)
    except ReplayError:
        raise
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError, ValueError) as error:
        raise ReplayError(f"{where} JSON") from error


@dataclass(frozen=True)
class ReplayDocument:
    header: dict[str, Any]
    events: tuple[dict[str, Any], ...]
    footer: dict[str, Any]


@dataclass(frozen=True)
class CadenceCensus:
    document: dict[str, Any]
    file_sha256: str
    payload_sha256: str
    source_hz: int
    native_hz: int
    hold: int
    record_count: int
    interval_count: int


def canonical_line(record: dict[str, Any]) -> bytes:
    return (json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n").encode()


def _read_regular_bounded(path: Path, maximum_bytes: int, where: str) -> bytes:
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NONBLOCK)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise ReplayError(f"{where} is not a regular file")
        with os.fdopen(descriptor, "rb") as source:
            descriptor = -1
            data = source.read(maximum_bytes + 1)
            if len(data) > maximum_bytes or source.read(1):
                raise ReplayError(f"{where} byte bound")
        return data
    except OSError as error:
        raise ReplayError(f"{where} unreadable: {error}") from error
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def _sha256_file_bounded(path: Path, maximum_bytes: int, where: str) -> str:
    return hashlib.sha256(_read_regular_bounded(path, maximum_bytes, where)).hexdigest()


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


def _fsync_directory(path: Path) -> None:
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY)
        os.fsync(descriptor)
    except OSError as error:
        raise ReplayError(f"output directory sync failed: {error}") from error
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def _publish_atomic_files(files: Sequence[tuple[Path, bytes, int]]) -> None:
    """Publish ordered, individually atomic files without overwriting.

    For the projection pair the replay is first and the receipt is last.  Each
    destination directory is synced before the next link, so a durable receipt
    is a commit marker for an already durable replay.  A process crash may
    leave an uncommitted replay orphan; two independent pathnames cannot form a
    single filesystem transaction.
    """
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
            _fsync_directory(destination.parent)
        for _, temporary in prepared:
            temporary.unlink()
        for directory in {temporary.parent for _, temporary in prepared}:
            _fsync_directory(directory)
    except BaseException:
        for destination in published:
            destination.unlink(missing_ok=True)
        for directory in {destination.parent for destination in published}:
            _fsync_directory(directory)
        for _, temporary in prepared:
            temporary.unlink(missing_ok=True)
        for directory in {temporary.parent for _, temporary in prepared}:
            _fsync_directory(directory)
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


def validate_target(value: object, where: str = "target") -> dict[str, Any]:
    target = require_dict(value, TARGET_KEYS, where)
    if not isinstance(target["title_id"], str) or HEX32.fullmatch(target["title_id"]) is None:
        raise ReplayError(f"{where} title_id")
    if not isinstance(target["media_id"], str) or HEX32.fullmatch(target["media_id"]) is None:
        raise ReplayError(f"{where} media_id")
    module = require_bounded_string(target["module"], MAX_MODULE_LENGTH, f"{where} module")
    if "/" in module or "\\" in module:
        raise ReplayError(f"{where} module basename")
    require_sha256(target["xex_sha256"], f"{where} xex")
    for field in ("xex_version", "base_version"):
        if not isinstance(target[field], str) or XEX_VERSION.fullmatch(target[field]) is None:
            raise ReplayError(f"{where} {field}")
    if not isinstance(target["module_xxh3"], str) or HEX64.fullmatch(target["module_xxh3"]) is None:
        raise ReplayError(f"{where} module_xxh3")
    if not isinstance(target["marker_address"], str) or HEX32.fullmatch(target["marker_address"]) is None:
        raise ReplayError(f"{where} marker_address")
    code_offset = require_uint(target["marker_code_offset"], 0xFFFFFFFF, f"{where} marker_code_offset")
    code_length = require_uint(target["marker_code_length"], MAX_MARKER_CODE_BYTES, f"{where} marker_code_length")
    if code_length == 0 or code_offset > 0xFFFFFFFF - code_length + 1:
        raise ReplayError(f"{where} marker code range")
    require_sha256(target["marker_code_sha256"], f"{where} marker code")
    for field, expected in PAL_TARGET_IDENTITY.items():
        if target[field] != expected:
            raise ReplayError(f"{where} PAL {field}")
    return target


def validate_oracle_target_v4(value: object, where: str = "target") -> dict[str, Any]:
    target = require_dict(value, TARGET_V4_KEYS, where)
    for field in ("title_id", "media_id", "entry_point", "region_mask"):
        if not isinstance(target[field], str) or HEX32.fullmatch(target[field]) is None:
            raise ReplayError(f"{where} {field}")
    if not isinstance(target["module_xxh3"], str) or HEX64_UPPER.fullmatch(target["module_xxh3"]) is None:
        raise ReplayError(f"{where} module_xxh3")
    module = require_bounded_string(target["module"], MAX_MODULE_LENGTH, f"{where} module")
    if "/" in module or "\\" in module:
        raise ReplayError(f"{where} module basename")
    require_bounded_string(target["target_id"], MAX_PLATFORM_LENGTH, f"{where} target_id")
    require_sha256(target["xex_sha256"], f"{where} xex")
    for field in ("xex_version", "base_version"):
        if not isinstance(target[field], str) or XEX_VERSION.fullmatch(target[field]) is None:
            raise ReplayError(f"{where} {field}")
    for field, expected in NTSC_UJ_ORACLE_TARGET_IDENTITY.items():
        if target[field] != expected:
            raise ReplayError(f"{where} NTSC-U/J {field}")
    return target


def _validate_marker_contract_v4(value: object, where: str) -> dict[str, Any]:
    marker = require_dict(value, MARKER_CONTRACT_KEYS, where)
    if marker["role"] not in MARKER_ROLES or marker["phase"] not in MARKER_PHASES:
        raise ReplayError(f"{where} role/phase")
    if not isinstance(marker["address"], str) or HEX32.fullmatch(marker["address"]) is None:
        raise ReplayError(f"{where} address")
    code = require_dict(marker["code"], MARKER_CODE_V4_KEYS, f"{where} code")
    if not isinstance(code["image_rva"], str) or HEX32.fullmatch(code["image_rva"]) is None:
        raise ReplayError(f"{where} code image_rva")
    code_length = require_uint(code["length"], MAX_MARKER_CODE_BYTES, f"{where} code length")
    require_sha256(code["sha256"], f"{where} code")
    if code_length == 0:
        raise ReplayError(f"{where} code range")
    if marker != NTSC_UJ_MARKER_CONTRACT:
        raise ReplayError(f"{where} NTSC-U/J identity")
    return marker


def _require_reduced_rational(value: object, where: str) -> tuple[int, int]:
    rational = require_dict(value, RATIONAL_KEYS, where)
    numerator = require_uint(rational["numerator"], 0xFFFFFFFFFFFFFFFF, f"{where} numerator")
    denominator = require_uint(rational["denominator"], 0xFFFFFFFFFFFFFFFF, f"{where} denominator")
    if numerator == 0 or denominator == 0 or math.gcd(numerator, denominator) != 1:
        raise ReplayError(f"{where} canonical rational")
    return numerator, denominator


def _reduced_rational(numerator: int, denominator: int) -> tuple[int, int]:
    divisor = math.gcd(numerator, denominator)
    return numerator // divisor, denominator // divisor


def _native_clock_contract(value: object, where: str) -> dict[str, Any]:
    contract = require_dict(value, NATIVE_CLOCK_KEYS, where)
    frequency = contract["frequency"]
    numerator, denominator = _require_reduced_rational(frequency, f"{where} frequency")
    if (
        contract["schema"] != NATIVE_CLOCK_SCHEMA
        or contract["clock_id"] != "ac6_native_fixed_step"
        or contract["tick_semantics"] != "one_simulation_step"
        or numerator != 60
        or denominator != 1
    ):
        raise ReplayError(f"{where} identity")
    return contract


def _marker_contract(header: dict[str, Any]) -> dict[str, Any]:
    if header["schema"] == SCHEMA_V4:
        return copy.deepcopy(header["sync"]["marker_contract"])
    return {
        "role": header["sync"]["marker_role"],
        "address": header["target"]["marker_address"],
        "phase": header["sync"]["marker_phase"],
        "code": {
            "offset": header["target"]["marker_code_offset"],
            "length": header["target"]["marker_code_length"],
            "sha256": header["target"]["marker_code_sha256"],
        },
    }


def _marker_phase(header: dict[str, Any]) -> str:
    if header["schema"] == SCHEMA_V4:
        return header["sync"]["marker_contract"]["phase"]
    return header["sync"]["marker_phase"]


def validate_header(header: object) -> dict[str, Any]:
    result = require_dict(header, HEADER_KEYS, "header")
    schema = result["schema"]
    if result["kind"] != "header" or not isinstance(schema, str) or schema not in {SCHEMA_V3, SCHEMA_V4}:
        raise ReplayError("header identity")

    producer = require_dict(result["producer"], PRODUCER_KEYS, "producer")
    if producer["lane"] not in LANES:
        raise ReplayError("producer lane")
    if not isinstance(producer["implementation_commit"], str) or (
        COMMIT.fullmatch(producer["implementation_commit"]) is None
    ):
        raise ReplayError("producer commit")
    require_sha256(producer["binary_sha256"], "producer binary")
    require_sha256(producer["build_sha256"], "producer build")
    require_bounded_string(producer["platform"], MAX_PLATFORM_LENGTH, "producer platform")

    if schema == SCHEMA_V3:
        validate_target(result["target"])
    else:
        validate_oracle_target_v4(result["target"])

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
        parent_start = require_uint(segment["parent_start_marker"], MAX_MARKERS, "segment parent_start_marker")
        marker_count = require_uint(segment["parent_marker_count"], MAX_MARKERS, "segment parent_marker_count")
        if parent_start == 0 or marker_count == 0 or parent_start > MAX_MARKERS - marker_count + 1:
            raise ReplayError("segment parent marker window")

    sync = require_dict(result["sync"], SYNC_KEYS if schema == SCHEMA_V3 else SYNC_V4_KEYS, "sync")
    if sync["primary"] != PRIMARY_SYNC_KEY:
        raise ReplayError("sync primary")
    if sync["portable_guards"] != list(PORTABLE_GUARDS):
        raise ReplayError("sync portable guards")
    if sync["lane_local_diagnostics"] != list(LANE_LOCAL_DIAGNOSTICS):
        raise ReplayError("sync lane-local diagnostics")
    if sync["telemetry"] != list(TELEMETRY):
        raise ReplayError("sync telemetry")
    if schema == SCHEMA_V3:
        if sync["marker_role"] not in MARKER_ROLES:
            raise ReplayError("sync marker role")
        if sync["marker_phase"] not in MARKER_PHASES:
            raise ReplayError("sync marker phase")
    else:
        _validate_marker_contract_v4(sync["marker_contract"], "sync marker contract")
    cadence = require_dict(sync["cadence"], CADENCE_KEYS, "sync cadence")
    status = cadence["status"]
    source_hz = cadence["source_hz"]
    if cadence["projection"] != PROJECTION:
        raise ReplayError("sync cadence projection")
    if status not in CADENCE_STATUSES or cadence["resampling"] not in RESAMPLING_POLICIES:
        raise ReplayError("sync cadence policy")
    if status == "unqualified":
        if source_hz is not None or cadence["native_hz"] is not None or cadence["resampling"] != "refuse":
            raise ReplayError("sync unqualified cadence")
        if (
            cadence["integrity_level"] is not None
            or cadence["census"] is not None
            or cadence["native_clock"] is not None
        ):
            raise ReplayError("sync unqualified cadence metadata")
    else:
        if cadence["integrity_level"] != CADENCE_INTEGRITY_LEVEL:
            raise ReplayError("sync cadence integrity level")
        source_hz = require_uint(source_hz, 1000, "sync cadence source_hz")
        native_hz = require_uint(cadence["native_hz"], 1000, "sync cadence native_hz")
        if source_hz == 0 or native_hz == 0:
            raise ReplayError("sync cadence source_hz")
        expected_resampling = "identity" if source_hz == native_hz else "zero_order_hold"
        if cadence["resampling"] != expected_resampling or native_hz < source_hz or native_hz % source_hz != 0:
            raise ReplayError("sync derived cadence")
        census = require_dict(cadence["census"], CENSUS_REFERENCE_KEYS, "sync cadence census")
        expected_census_schema = CADENCE_CENSUS_SCHEMA_V1 if schema == SCHEMA_V3 else CADENCE_CENSUS_SCHEMA_V2
        if census["schema"] != expected_census_schema:
            raise ReplayError("sync cadence census schema")
        require_sha256(census["file_sha256"], "sync cadence census file")
        require_sha256(census["payload_sha256"], "sync cadence census payload")
        if census["integrity_level"] != CADENCE_INTEGRITY_LEVEL or census["method"] != CADENCE_METHOD:
            raise ReplayError("sync cadence census identity")
        record_count = require_uint(census["record_count"], MAX_CADENCE_RECORDS, "sync cadence record_count")
        interval_count = require_uint(census["interval_count"], MAX_CADENCE_RECORDS - 1, "sync cadence interval_count")
        if (
            record_count < 2
            or interval_count != record_count - 1
            or (segment["kind"] == "marker_window" and record_count != segment["parent_marker_count"])
        ):
            raise ReplayError("sync cadence census intervals")
        native_clock = _native_clock_contract(cadence["native_clock"], "sync native clock")
        if native_hz != native_clock["frequency"]["numerator"]:
            raise ReplayError("sync native cadence")
    if segment["kind"] == "full_recording" and status != "unqualified":
        raise ReplayError("full recording cadence must be unqualified")
    if segment["kind"] == "marker_window" and status != "derived":
        raise ReplayError("marker window cadence must be derived")
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
            if require_uint(event["sequence"], MAX_EVENTS - 1, f"event {sequence} sequence") != sequence:
                raise ReplayError(f"event {sequence} sequence")
            if require_uint(event["marker_index"], MAX_MARKERS, f"event {sequence} marker_index") != expected_marker:
                raise ReplayError(f"event {sequence} marker_index")
            if require_uint(event["poll_index"], MAX_POLLS, f"event {sequence} poll_index") != expected_poll:
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
            if require_uint(event["sequence"], MAX_EVENTS - 1, f"event {sequence} sequence") != sequence:
                raise ReplayError(f"event {sequence} sequence")
            if require_uint(event["poll_index"], MAX_POLLS - 1, f"event {sequence} poll_index") != expected_poll:
                raise ReplayError(f"event {sequence} poll_index")
            if require_uint(event["marker_index"], MAX_MARKERS, f"event {sequence} marker_index") != current_marker:
                raise ReplayError(f"event {sequence} poll marker_index")
            if (
                require_uint(event["poll_in_marker"], MAX_POLLS - 1, f"event {sequence} poll_in_marker")
                != expected_poll_in_marker
            ):
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
    validated_events = validate_events(events, _marker_phase(validated_header))
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
        record = _strict_json_loads(raw_line, f"line {line_number}")
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
    events = validate_events(records[1:-1], _marker_phase(header))
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


def _validate_cadence_census_document(value: object, file_sha256: str) -> CadenceCensus:
    census = require_dict(value, CENSUS_KEYS, "cadence census")
    census_schema = census["schema"]
    if (
        census["kind"] != "cadence_census"
        or not isinstance(census_schema, str)
        or census_schema
        not in {
            CADENCE_CENSUS_SCHEMA_V1,
            CADENCE_CENSUS_SCHEMA_V2,
        }
    ):
        raise ReplayError("cadence census identity")
    if census["integrity_level"] != CADENCE_INTEGRITY_LEVEL:
        raise ReplayError("cadence census integrity level")
    require_sha256(census["payload_sha256"], "cadence census payload")
    body = {key: census[key] for key in CENSUS_BODY_KEYS}
    payload_sha256 = hashlib.sha256(canonical_line(body)).hexdigest()
    if census["payload_sha256"] != payload_sha256:
        raise ReplayError("cadence census payload_sha256")

    producer = require_dict(census["producer"], PRODUCER_KEYS, "cadence census producer")
    if producer["lane"] not in LANES:
        raise ReplayError("cadence census producer lane")
    if (
        not isinstance(producer["implementation_commit"], str)
        or COMMIT.fullmatch(producer["implementation_commit"]) is None
    ):
        raise ReplayError("cadence census producer commit")
    require_sha256(producer["binary_sha256"], "cadence census producer binary")
    require_sha256(producer["build_sha256"], "cadence census producer build")
    require_bounded_string(producer["platform"], MAX_PLATFORM_LENGTH, "cadence census producer platform")

    configuration = require_dict(census["configuration"], CENSUS_CONFIGURATION_KEYS, "cadence census configuration")
    require_sha256(configuration["runtime_config_sha256"], "cadence census runtime configuration")
    require_sha256(configuration["behavior_config_sha256"], "cadence census behavior configuration")

    parent = require_dict(census["parent"], CENSUS_PARENT_KEYS, "cadence census parent")
    require_sha256(parent["replay_sha256"], "cadence census parent replay")
    require_sha256(parent["payload_sha256"], "cadence census parent payload")
    window = require_dict(parent["window"], CENSUS_WINDOW_KEYS, "cadence census parent window")
    start_marker = require_uint(window["start_marker"], MAX_MARKERS, "cadence census start_marker")
    marker_count = require_uint(window["marker_count"], MAX_MARKERS, "cadence census marker_count")
    _require_marker_window(start_marker, marker_count, "cadence census")

    if census_schema == CADENCE_CENSUS_SCHEMA_V1:
        target = validate_target(census["target"], "cadence census target")
        marker = require_dict(census["marker_contract"], MARKER_CONTRACT_KEYS, "cadence census marker contract")
        if marker["role"] not in MARKER_ROLES or marker["phase"] not in MARKER_PHASES:
            raise ReplayError("cadence census marker role/phase")
        if not isinstance(marker["address"], str) or HEX32.fullmatch(marker["address"]) is None:
            raise ReplayError("cadence census marker address")
        code = require_dict(marker["code"], MARKER_CODE_KEYS, "cadence census marker code")
        code_offset = require_uint(code["offset"], 0xFFFFFFFF, "cadence census marker code offset")
        code_length = require_uint(code["length"], MAX_MARKER_CODE_BYTES, "cadence census marker code length")
        require_sha256(code["sha256"], "cadence census marker code")
        if code_length == 0 or code_offset > 0xFFFFFFFF - code_length + 1:
            raise ReplayError("cadence census marker code range")
        if (
            marker["address"] != target["marker_address"]
            or code["offset"] != target["marker_code_offset"]
            or code["length"] != target["marker_code_length"]
            or code["sha256"] != target["marker_code_sha256"]
        ):
            raise ReplayError("cadence census marker target mismatch")
    else:
        validate_oracle_target_v4(census["target"], "cadence census target")
        _validate_marker_contract_v4(census["marker_contract"], "cadence census marker contract")

    clock = require_dict(census["clock"], REFERENCE_CLOCK_KEYS, "cadence census clock")
    if (
        clock["schema"] != REFERENCE_CLOCK_SCHEMA
        or clock["counter_bits"] != 64
        or clock["read_semantics"] != "monotonic_snapshot_before_marker"
    ):
        raise ReplayError("cadence census clock identity")
    require_bounded_string(clock["clock_id"], MAX_PLATFORM_LENGTH, "cadence census clock id")
    reference_hz_numerator, reference_hz_denominator = _require_reduced_rational(
        clock["frequency"], "cadence census clock frequency"
    )
    if reference_hz_numerator > 1_000_000_000 or reference_hz_denominator > 1_000_000_000:
        raise ReplayError("cadence census clock frequency bound")

    records = census["records"]
    if not isinstance(records, list) or not 2 <= len(records) <= MAX_CADENCE_RECORDS:
        raise ReplayError("cadence census record count")
    if len(records) != marker_count:
        raise ReplayError("cadence census record/window mismatch")
    ticks: list[int] = []
    previous_event_sequence = -1
    for sequence, record_value in enumerate(records):
        record = require_dict(record_value, CENSUS_RECORD_KEYS, f"cadence census record {sequence}")
        parent_marker_index = start_marker + sequence
        record_sequence = require_uint(
            record["sequence"], MAX_CADENCE_RECORDS - 1, f"cadence census record {sequence} sequence"
        )
        observed_parent_marker = require_uint(
            record["parent_marker_index"], MAX_MARKERS, f"cadence census record {sequence} parent_marker_index"
        )
        if record_sequence != sequence or observed_parent_marker != parent_marker_index:
            raise ReplayError(f"cadence census record {sequence} identity")
        event_sequence = require_uint(
            record["event_sequence"], MAX_EVENTS - 1, f"cadence census record {sequence} event_sequence"
        )
        reference_tick = require_uint(
            record["reference_tick"], 0xFFFFFFFFFFFFFFFF, f"cadence census record {sequence} reference_tick"
        )
        if event_sequence <= previous_event_sequence or (ticks and reference_tick <= ticks[-1]):
            raise ReplayError(f"cadence census record {sequence} order")
        previous_event_sequence = event_sequence
        ticks.append(reference_tick)

    interval_count = len(ticks) - 1
    intervals = [right - left for left, right in zip(ticks, ticks[1:])]
    if not intervals or any(interval != intervals[0] for interval in intervals[1:]):
        raise ReplayError("cadence census non-uniform intervals")
    elapsed_ticks = ticks[-1] - ticks[0]
    source_numerator = reference_hz_numerator * interval_count
    source_denominator = reference_hz_denominator * elapsed_ticks
    source_numerator, source_denominator = _reduced_rational(source_numerator, source_denominator)
    if source_denominator != 1 or not 0 < source_numerator <= 1000:
        raise ReplayError("cadence census non-integral source rate")
    source_hz = source_numerator

    native_frequency = NATIVE_CLOCK_CONTRACT["frequency"]
    native_numerator, native_denominator = _require_reduced_rational(
        native_frequency, "native simulation clock frequency"
    )
    if native_denominator != 1:
        raise ReplayError("native simulation clock non-integral rate")
    native_hz = native_numerator
    if native_hz < source_hz or native_hz % source_hz != 0:
        raise ReplayError("cadence census unsupported native ratio")
    hold = native_hz // source_hz
    if (source_hz, native_hz, hold) not in SUPPORTED_CADENCES:
        raise ReplayError("cadence census unsupported rate")

    require_sha256(file_sha256, "cadence census file")
    return CadenceCensus(
        census,
        file_sha256,
        payload_sha256,
        source_hz,
        native_hz,
        hold,
        len(records),
        interval_count,
    )


def seal_cadence_census(body: dict[str, Any]) -> bytes:
    require_dict(body, CENSUS_BODY_KEYS, "cadence census body")
    census = copy.deepcopy(body)
    census["payload_sha256"] = hashlib.sha256(canonical_line(census)).hexdigest()
    data = canonical_line(census)
    if len(data) > MAX_CADENCE_CENSUS_BYTES:
        raise ReplayError("cadence census byte bound")
    _validate_cadence_census_document(census, hashlib.sha256(data).hexdigest())
    return data


def load_cadence_census_bytes(data: bytes) -> CadenceCensus:
    if not data or len(data) > MAX_CADENCE_CENSUS_BYTES:
        raise ReplayError("cadence census byte bound")
    if not data.endswith(b"\n") or b"\r" in data or data.count(b"\n") != 1:
        raise ReplayError("cadence census framing")
    value = _strict_json_loads(data, "cadence census")
    if not isinstance(value, dict) or canonical_line(value) != data:
        raise ReplayError("cadence census canonical encoding")
    return _validate_cadence_census_document(value, hashlib.sha256(data).hexdigest())


def _require_census_contract(census: CadenceCensus, header: dict[str, Any]) -> None:
    expected_schema = CADENCE_CENSUS_SCHEMA_V1 if header["schema"] == SCHEMA_V3 else CADENCE_CENSUS_SCHEMA_V2
    if census.document["schema"] != expected_schema:
        raise ReplayError("cadence census/raw schema mismatch")
    if census.document["producer"] != header["producer"]:
        raise ReplayError("cadence census producer mismatch")
    expected_configuration = {
        "runtime_config_sha256": header["session"]["runtime_config_sha256"],
        "behavior_config_sha256": header["session"]["behavior_config_sha256"],
    }
    if census.document["configuration"] != expected_configuration:
        raise ReplayError("cadence census configuration mismatch")
    if census.document["target"] != header["target"]:
        raise ReplayError("cadence census target mismatch")
    if census.document["marker_contract"] != _marker_contract(header):
        raise ReplayError("cadence census marker contract mismatch")


def _require_census_records_for_parent(census: CadenceCensus, parent: ReplayDocument) -> None:
    window = census.document["parent"]["window"]
    start_marker, end_marker = _require_marker_window(
        window["start_marker"], window["marker_count"], "cadence census records"
    )
    markers = [
        event
        for event in parent.events
        if event["kind"] == "marker" and start_marker <= event["marker_index"] < end_marker
    ]
    if len(markers) != census.record_count:
        raise ReplayError("cadence census parent marker coverage")
    for record, marker in zip(census.document["records"], markers):
        if record["parent_marker_index"] != marker["marker_index"] or record["event_sequence"] != marker["sequence"]:
            raise ReplayError("cadence census parent event mismatch")


def _census_reference(census: CadenceCensus) -> dict[str, Any]:
    return {
        "schema": census.document["schema"],
        "file_sha256": census.file_sha256,
        "payload_sha256": census.payload_sha256,
        "integrity_level": CADENCE_INTEGRITY_LEVEL,
        "method": CADENCE_METHOD,
        "record_count": census.record_count,
        "interval_count": census.interval_count,
    }


def slice_replay(
    parent_replay: bytes,
    start_marker: int,
    marker_count: int,
    cadence_census: bytes,
    expected_header: dict[str, Any] | None,
) -> bytes:
    if len(parent_replay) > MAX_REPLAY_BYTES:
        raise ReplayError("parent replay byte bound")
    if expected_header is None:
        raise ReplayError("slice requires an expected-header marker contract")
    parent = load_replay_bytes(parent_replay, expected_header)
    if parent.header["segment"]["kind"] != "full_recording":
        raise ReplayError("slice parent must be a full recording")
    parent_replay_sha256 = hashlib.sha256(parent_replay).hexdigest()
    start_marker, end_marker = _require_marker_window(start_marker, marker_count, "slice")
    census = load_cadence_census_bytes(cadence_census)
    _require_census_contract(census, parent.header)
    census_parent = census.document["parent"]
    if census_parent["replay_sha256"] != parent_replay_sha256:
        raise ReplayError("cadence census parent replay mismatch")
    if census_parent["payload_sha256"] != parent.footer["payload_sha256"]:
        raise ReplayError("cadence census parent payload mismatch")
    if census_parent["window"] != {"start_marker": start_marker, "marker_count": marker_count}:
        raise ReplayError("cadence census parent window mismatch")
    _require_census_records_for_parent(census, parent)

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
        "status": "derived",
        "integrity_level": CADENCE_INTEGRITY_LEVEL,
        "source_hz": census.source_hz,
        "native_hz": census.native_hz,
        "resampling": "identity" if census.hold == 1 else "zero_order_hold",
        "projection": PROJECTION,
        "census": _census_reference(census),
        "native_clock": copy.deepcopy(NATIVE_CLOCK_CONTRACT),
    }
    return seal_replay(sliced_header, sliced_events)


def _require_census_for_window(census: CadenceCensus, document: ReplayDocument) -> None:
    _require_census_contract(census, document.header)
    segment = document.header["segment"]
    census_parent = census.document["parent"]
    expected_parent = {
        "replay_sha256": segment["parent_replay_sha256"],
        "payload_sha256": segment["parent_payload_sha256"],
        "window": {
            "start_marker": segment["parent_start_marker"],
            "marker_count": segment["parent_marker_count"],
        },
    }
    if census_parent != expected_parent:
        raise ReplayError("cadence census window lineage mismatch")
    cadence = document.header["sync"]["cadence"]
    if cadence["census"] != _census_reference(census):
        raise ReplayError("cadence census reference mismatch")
    if (
        cadence["integrity_level"] != CADENCE_INTEGRITY_LEVEL
        or cadence["native_clock"] != NATIVE_CLOCK_CONTRACT
        or cadence["source_hz"] != census.source_hz
        or cadence["native_hz"] != census.native_hz
    ):
        raise ReplayError("cadence census computed contract mismatch")


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
    if cadence["status"] != "derived" or cadence["integrity_level"] != CADENCE_INTEGRITY_LEVEL:
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


def export_controller_tsv(
    document: ReplayDocument,
    start_marker: int,
    marker_count: int,
    cadence_census: bytes,
) -> str:
    if document.header["segment"]["kind"] != "marker_window":
        raise ReplayError("controller export requires a resealed marker window")
    census = load_cadence_census_bytes(cadence_census)
    _require_census_for_window(census, document)
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
    data = _read_regular_bounded(current, CACHE_CURRENT_SIZE, "cache current record")
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


def _build_ac6rtply(
    raw_replay: bytes,
    cache_index_sha256: str,
    cadence_census: bytes,
    expected_header: dict[str, Any] | None,
    required_raw_schema: str,
) -> tuple[bytes, dict[str, Any]]:
    if len(raw_replay) > MAX_REPLAY_BYTES:
        raise ReplayError("projection raw replay byte bound")
    document = load_replay_bytes(raw_replay, expected_header)
    if document.header["schema"] != required_raw_schema:
        raise ReplayError(f"AC6RTPLY projection requires raw {required_raw_schema}")
    raw_replay_sha256 = hashlib.sha256(raw_replay).hexdigest()
    require_sha256(cache_index_sha256, "projection cache index")
    if not any(bytes.fromhex(cache_index_sha256)):
        raise ReplayError("projection cache index is zero")
    segment = document.header["segment"]
    if segment["kind"] != "marker_window":
        raise ReplayError("AC6RTPLY projection requires a resealed marker window")
    census = load_cadence_census_bytes(cadence_census)
    _require_census_for_window(census, document)

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
    source = {
        "raw_replay_sha256": raw_replay_sha256,
        "raw_payload_sha256": document.footer["payload_sha256"],
        "parent_replay_sha256": segment["parent_replay_sha256"],
        "parent_payload_sha256": segment["parent_payload_sha256"],
        "parent_window": {
            "start_marker": segment["parent_start_marker"],
            "marker_count": segment["parent_marker_count"],
        },
    }
    receipt_cadence = {
        "integrity_level": CADENCE_INTEGRITY_LEVEL,
        "source_hz": cadence["source_hz"],
        "native_hz": cadence["native_hz"],
        "resampling": cadence["resampling"],
        "hold": hold,
        "census": copy.deepcopy(cadence["census"]),
        "native_clock": copy.deepcopy(NATIVE_CLOCK_CONTRACT),
    }
    receipt_output = {
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
    }
    if required_raw_schema == SCHEMA_V3:
        receipt_cadence["marker_contract"] = copy.deepcopy(census.document["marker_contract"])
        receipt = {
            "kind": "native_projection_receipt",
            "schema": PROJECTION_RECEIPT_SCHEMA_V3,
            "source": source,
            "target": copy.deepcopy(document.header["target"]),
            "cadence": receipt_cadence,
            "mapping": copy.deepcopy(CONTROLLER_MAPPING),
            "cache_index_sha256": cache_index_sha256,
            "output": receipt_output,
        }
    else:
        source["raw_schema"] = SCHEMA_V4
        source["oracle"] = {
            "target": copy.deepcopy(document.header["target"]),
            "marker_contract": copy.deepcopy(document.header["sync"]["marker_contract"]),
        }
        receipt = {
            "kind": "native_projection_receipt",
            "schema": PROJECTION_RECEIPT_SCHEMA_V4,
            "source": source,
            "native_target": copy.deepcopy(PAL_NATIVE_TARGET_IDENTITY),
            "cadence": receipt_cadence,
            "mapping": copy.deepcopy(CONTROLLER_MAPPING),
            "cache_index_sha256": cache_index_sha256,
            "output": receipt_output,
        }
    receipt_bytes = canonical_line(receipt)
    if len(receipt_bytes) > MAX_RECEIPT_BYTES:
        raise ReplayError("projection receipt byte bound")
    return replay, receipt


def build_ac6rtply_v3(
    raw_replay: bytes,
    cache_index_sha256: str,
    cadence_census: bytes,
    expected_header: dict[str, Any] | None = None,
) -> tuple[bytes, dict[str, Any]]:
    return _build_ac6rtply(raw_replay, cache_index_sha256, cadence_census, expected_header, SCHEMA_V3)


def build_ac6rtply_v4(
    raw_replay: bytes,
    cache_index_sha256: str,
    cadence_census: bytes,
    expected_header: dict[str, Any] | None = None,
) -> tuple[bytes, dict[str, Any]]:
    return _build_ac6rtply(raw_replay, cache_index_sha256, cadence_census, expected_header, SCHEMA_V4)


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
        value = _strict_json_loads(b"".join(lines), "input")
    except (OSError, ReplayError) as error:
        raise ReplayError(f"JSON unreadable: {path}") from error
    if not isinstance(value, dict):
        raise ReplayError(f"JSON object required: {path}")
    return value


def _read_bounded_file_bytes(path: Path, maximum_bytes: int, where: str) -> bytes:
    return _read_regular_bounded(path, maximum_bytes, where)


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    values: list[dict[str, Any]] = []
    try:
        for line_number, raw_line in enumerate(_read_bounded_binary_lines(path, MAX_EVENTS), 1):
            if not raw_line.strip():
                continue
            value = _strict_json_loads(raw_line, f"event line {line_number}")
            if not isinstance(value, dict):
                raise ReplayError(f"event line {line_number} shape")
            values.append(value)
    except (OSError, ReplayError) as error:
        raise ReplayError(f"events unreadable: {path}") from error
    return values


def _read_bounded_binary_lines(path: Path, maximum_records: int) -> list[bytes]:
    data = _read_regular_bounded(path, MAX_REPLAY_BYTES, "input")
    lines = data.splitlines(keepends=True)
    for line_number, raw_line in enumerate(lines, 1):
        if len(raw_line) > MAX_LINE_BYTES:
            raise ReplayError(f"input line {line_number} exceeds byte bound")
        if line_number > maximum_records:
            raise ReplayError(f"input exceeds record bound: {path}")
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
    slice_parser.add_argument("--cadence-census", type=Path, required=True)
    slice_parser.add_argument("--start-marker", type=int, required=True)
    slice_parser.add_argument("--marker-count", type=int, required=True)

    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("replay", type=Path)
    verify_parser.add_argument("--expected-header", type=Path)

    export_parser = subparsers.add_parser("export-controller-tsv")
    export_parser.add_argument("replay", type=Path)
    export_parser.add_argument("output", type=Path)
    export_parser.add_argument("--expected-header", type=Path, required=True)
    export_parser.add_argument("--cadence-census", type=Path, required=True)
    export_parser.add_argument("--start-marker", type=int, default=1)
    export_parser.add_argument("--marker-count", type=int, required=True)

    project_parser = subparsers.add_parser("project-ac6rtply-v3")
    project_parser.add_argument("replay", type=Path)
    project_parser.add_argument("cache", type=Path)
    project_parser.add_argument("output", type=Path)
    project_parser.add_argument("receipt", type=Path)
    project_parser.add_argument("--expected-header", type=Path, required=True)
    project_parser.add_argument("--cadence-census", type=Path, required=True)

    project_v4_parser = subparsers.add_parser("project-ac6rtply-v4")
    project_v4_parser.add_argument("replay", type=Path)
    project_v4_parser.add_argument("cache", type=Path)
    project_v4_parser.add_argument("output", type=Path)
    project_v4_parser.add_argument("receipt", type=Path)
    project_v4_parser.add_argument("--expected-header", type=Path, required=True)
    project_v4_parser.add_argument("--cadence-census", type=Path, required=True)

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
                _read_bounded_file_bytes(arguments.cadence_census, MAX_CADENCE_CENSUS_BYTES, "cadence census"),
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
            output = export_controller_tsv(
                document,
                arguments.start_marker,
                arguments.marker_count,
                _read_bounded_file_bytes(arguments.cadence_census, MAX_CADENCE_CENSUS_BYTES, "cadence census"),
            )
            output_bytes = output.encode()
            _atomic_write_new(arguments.output, output_bytes, MAX_PROJECTED_TSV_BYTES)
            print(
                "controller_replay=exported "
                f"rows={len(output.splitlines())} "
                f"sha256={hashlib.sha256(output_bytes).hexdigest()}"
            )
        elif arguments.command in {"project-ac6rtply-v3", "project-ac6rtply-v4"}:
            expected_header = _read_json(arguments.expected_header)
            raw_data, _ = _load_replay_file_bytes(arguments.replay, expected_header)
            cache_index_sha256 = read_cache_identity(arguments.cache)
            build_projection = build_ac6rtply_v3 if arguments.command == "project-ac6rtply-v3" else build_ac6rtply_v4
            output, receipt = build_projection(
                raw_data,
                cache_index_sha256,
                _read_bounded_file_bytes(arguments.cadence_census, MAX_CADENCE_CENSUS_BYTES, "cadence census"),
                expected_header,
            )
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
