#!/usr/bin/env python3
"""Validate and seal a bounded Mission 01 oracle reference pack.

The script never reads PAC/DATA archives. It accepts only the four explicit
readbacks/replay files and writes the manifest consumed by ac6-native.
"""
from __future__ import annotations

import argparse
import hashlib
import math
import struct
from pathlib import Path


EXPECTED_TICKS = (1, 60, 300, 900, 1800)
FILES = ("replay.ac6rply", "checkpoints.tsv", "oracle-color.ppm", "oracle-depth.f32")


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def fnv64(path: Path) -> tuple[int, int]:
    digest = 1469598103934665603
    size = 0
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            size += len(chunk)
            for byte in chunk:
                digest ^= byte
                digest = (digest * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return size, digest


def read_ppm(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if not data.startswith(b"P6\n"):
        fail("oracle-color.ppm must be binary P6")
    parts = data.split(b"\n", 3)
    if len(parts) != 4 or parts[2] != b"255":
        fail("oracle-color.ppm header is invalid")
    try:
        width, height = (int(value) for value in parts[1].split())
    except ValueError:
        fail("oracle-color.ppm dimensions are invalid")
    if (width, height) != (1280, 720):
        fail("oracle-color.ppm must be 1280x720")
    if len(parts[3]) != width * height * 3:
        fail("oracle-color.ppm pixel payload size is invalid")
    return width, height


def validate_replay(path: Path) -> None:
    data = path.read_bytes()
    if data[:8] != b"AC6RPLY\0" or len(data) < 16:
        fail("replay.ac6rply magic is invalid")
    version, count = struct.unpack_from("<II", data, 8)
    if version != 1 or count != 1800 or len(data) != 16 + count * 9:
        fail("replay.ac6rply must contain exactly 1800 version-1 frames")


def validate_checkpoints(path: Path) -> None:
    rows = []
    for raw in path.read_text().splitlines():
        if not raw or raw.startswith("#"):
            continue
        fields = raw.split("\t")
        if len(fields) != 13:
            fail("checkpoint rows must contain 13 tab-separated fields")
        try:
            values = [float(value) for value in fields]
            tick = int(fields[0])
        except ValueError:
            fail("checkpoint contains a non-numeric field")
        if tick != values[0] or any(not math.isfinite(value) for value in values):
            fail("checkpoint contains an invalid value")
        rows.append(tick)
    if tuple(rows) != EXPECTED_TICKS:
        fail(f"checkpoint ticks must be {EXPECTED_TICKS}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference_dir", type=Path)
    args = parser.parse_args()
    directory = args.reference_dir.resolve()
    if not directory.is_dir():
        fail("reference directory does not exist")
    paths = {name: directory / name for name in FILES}
    if any(not path.is_file() for path in paths.values()):
        fail("all four reference files must exist")
    validate_replay(paths["replay.ac6rply"])
    validate_checkpoints(paths["checkpoints.tsv"])
    width, height = read_ppm(paths["oracle-color.ppm"])
    depth_size = paths["oracle-depth.f32"].stat().st_size
    if depth_size != width * height * 4:
        fail("oracle-depth.f32 must contain one float per pixel")
    for (value,) in struct.iter_unpack("<f", paths["oracle-depth.f32"].read_bytes()):
        if not math.isfinite(value) or not 0.0 <= value <= 1.0:
            fail("oracle-depth.f32 contains a value outside [0,1]")
    lines = ["version\t1", "mission_id\t1", "ticks\t1800", f"width\t{width}", f"height\t{height}"]
    for name in FILES:
        size, digest = fnv64(paths[name])
        lines.append(f"file\t{name}\t{size}\t{digest:016x}")
    temporary = directory / "reference.tsv.tmp"
    temporary.write_text("\n".join(lines) + "\n")
    temporary.replace(directory / "reference.tsv")
    print(f"sealed={directory / 'reference.tsv'}")
    print(f"sha256={hashlib.sha256((directory / 'reference.tsv').read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
