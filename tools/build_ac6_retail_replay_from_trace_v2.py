#!/usr/bin/env python3
"""Build a native AC6 retail replay from a qualified execution-trace v2 input stream."""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from build_ac6_execution_trace_v2 import TraceV2Error, load_jsonl


CURRENT_MAGIC = b"AC6RCUR\0"
CURRENT_VERSION = 2
CURRENT_SIZE = 48
REPLAY_MAGIC = b"AC6RTPLY\0"
LEGACY_REPLAY_VERSION = 2
MAXIMUM_FRAMES = 1_000_000
NATIVE_TICKS_PER_SAMPLE = 2
CONTROLLER_FIELDS = {"pitch", "roll", "yaw", "throttle", "buttons"}


class ReplayBuildError(ValueError):
    pass


def sha256(path: Path) -> bytes:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.digest()


def require_integer(payload: dict, name: str, minimum: int, maximum: int) -> int:
    value = payload.get(name)
    if not isinstance(value, int) or isinstance(value, bool) or not minimum <= value <= maximum:
        raise ReplayBuildError(f"controller {name} range")
    return value


def read_cache_identity(cache: Path) -> bytes:
    current = cache / "current"
    try:
        data = current.read_bytes()
    except OSError as error:
        raise ReplayBuildError("cache current record is absent") from error
    if len(data) != CURRENT_SIZE or data[:8] != CURRENT_MAGIC:
        raise ReplayBuildError("cache current record shape")
    version, size = struct.unpack_from(">II", data, 8)
    digest = data[16:48]
    if version != CURRENT_VERSION or size != CURRENT_SIZE or not any(digest):
        raise ReplayBuildError("cache current record identity")
    index = cache / "indices" / f"{digest.hex()}.ac6idx"
    if not index.is_file() or sha256(index) != digest:
        raise ReplayBuildError("sealed content index is absent or corrupt")
    return digest


def build_replay(raw_jsonl: Path, cache: Path, start_tick: int, tick_count: int) -> bytes:
    native_frame_count = tick_count * NATIVE_TICKS_PER_SAMPLE
    if (start_tick < 0 or tick_count <= 0 or
            native_frame_count > MAXIMUM_FRAMES):
        raise ReplayBuildError("replay window bounds")
    content_identity = read_cache_identity(cache)
    try:
        events = load_jsonl(raw_jsonl, start_tick, tick_count)
    except TraceV2Error as error:
        raise ReplayBuildError(str(error)) from error

    frames = bytearray()
    for event in events:
        if event["domain"] != "controller_input":
            continue
        payload = event["payload"]
        if set(payload) != CONTROLLER_FIELDS:
            raise ReplayBuildError(f"tick {event['tick']} controller shape")
        pitch = require_integer(payload, "pitch", -32768, 32767)
        roll = require_integer(payload, "roll", -32768, 32767)
        yaw = require_integer(payload, "yaw", -32768, 32767)
        throttle = require_integer(payload, "throttle", 0, 255)
        buttons = require_integer(payload, "buttons", 0, 65535)
        frame = struct.pack("<hhhBH", pitch, roll, yaw, throttle, buttons)
        frames.extend(frame * NATIVE_TICKS_PER_SAMPLE)
    if len(frames) != native_frame_count * 9:
        raise ReplayBuildError("controller frame count")

    header = bytearray(REPLAY_MAGIC)
    header.extend(struct.pack(
        "<IIIIII",
        LEGACY_REPLAY_VERSION,
        1,  # Mission 01.
        1,  # Normal difficulty.
        1,  # Qualified default aircraft.
        1,  # Qualified default weapon.
        1,  # Capability data is present in the sealed retail cache.
    ))
    header.extend(content_identity)
    header.extend(struct.pack("<I", native_frame_count))
    header.extend(frames)
    return bytes(header)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw_jsonl", type=Path)
    parser.add_argument("cache", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--start-tick", type=int, default=1)
    parser.add_argument("--tick-count", type=int, default=3600)
    arguments = parser.parse_args()
    try:
        replay = build_replay(
            arguments.raw_jsonl, arguments.cache,
            arguments.start_tick, arguments.tick_count,
        )
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        with arguments.output.open("xb") as destination:
            destination.write(replay)
    except (OSError, ReplayBuildError) as error:
        print(f"retail_replay_from_trace_v2=fail reason={error}")
        return 1
    print(f"retail_replay_from_trace_v2=pass samples={arguments.tick_count} "
          f"native_frames={arguments.tick_count * NATIVE_TICKS_PER_SAMPLE} "
          f"sha256={hashlib.sha256(replay).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
