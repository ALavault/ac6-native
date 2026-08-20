#!/usr/bin/env python3
"""Audit the reached AC6 PAL 640x360 -> 1280x720 copy/resolve profile.

The oracle is independent of the Vulkan compute shader used by the product:
- each 640x360 RGBA8 source pixel is replicated to a 2x2 destination block;
- copy_dest_swap=1 swaps R and B while preserving G and A;
- destination pixels are placed with the qualified Xenos 2D tiled mapping;
- 0x14000 non-pixel padding bytes remain 0xA5 in the compute audit buffer.

The tool reads caller-provided runtime artifacts and never writes game data.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

NORMAL_WIDTH = 640
NORMAL_HEIGHT = 360
NORMAL_BYTES = NORMAL_WIDTH * NORMAL_HEIGHT * 4
DEST_WIDTH = 1280
DEST_HEIGHT = 720
DEST_LINEAR_BYTES = DEST_WIDTH * DEST_HEIGHT * 4
DEST_TILED_BYTES = 0x398000
PADDING = 0xA5
EXPECTED_LINEAR_ASYMMETRIC = (
    "66dde082635ccc6b24abba5b372ceb10173bc2b062faa2d93de7c4548bb60dc8"
)
EXPECTED_TILED_ASYMMETRIC = (
    "0bf69cf42fd6c3ac73b30c438a4db6d1664eaafa9c716b9ba330a9886c976786"
)


def tiled_offset_2d(x: int, y: int, pitch: int = DEST_WIDTH) -> int:
    if not (0 <= x < DEST_WIDTH and 0 <= y < DEST_HEIGHT):
        raise ValueError("tiled coordinate outside reached destination")
    pitch = (pitch + 31) & ~31
    bpp_log2 = 2
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpp_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpp_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    result = (
        ((offset & ~0x1FF) << 3)
        + ((y & 16) << 7)
        + ((offset & 0x1C0) << 2)
        + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)
        + (offset & 0x3F)
    )
    if result < 0 or result + 4 > DEST_TILED_BYTES:
        raise ValueError("tiled offset outside reached allocation")
    return result


def build_linear_oracle(normal: bytes) -> bytes:
    if len(normal) != NORMAL_BYTES:
        raise ValueError(f"normal RGBA8 extent is {len(normal)}, expected {NORMAL_BYTES}")
    destination = bytearray(DEST_LINEAR_BYTES)
    for y in range(DEST_HEIGHT):
        source_row = (y >> 1) * NORMAL_WIDTH * 4
        destination_row = y * DEST_WIDTH * 4
        for x in range(DEST_WIDTH):
            source = source_row + (x >> 1) * 4
            target = destination_row + x * 4
            destination[target + 0] = normal[source + 2]
            destination[target + 1] = normal[source + 1]
            destination[target + 2] = normal[source + 0]
            destination[target + 3] = normal[source + 3]
    return bytes(destination)


def tile_linear(linear: bytes, padding: int = PADDING) -> bytes:
    if len(linear) != DEST_LINEAR_BYTES:
        raise ValueError("linear destination extent is invalid")
    tiled = bytearray([padding]) * DEST_TILED_BYTES
    for y in range(DEST_HEIGHT):
        row = y * DEST_WIDTH * 4
        for x in range(DEST_WIDTH):
            source = row + x * 4
            target = tiled_offset_2d(x, y)
            tiled[target : target + 4] = linear[source : source + 4]
    return bytes(tiled)


def untile(tiled: bytes) -> bytes:
    if len(tiled) != DEST_TILED_BYTES:
        raise ValueError("tiled destination extent is invalid")
    linear = bytearray(DEST_LINEAR_BYTES)
    for y in range(DEST_HEIGHT):
        row = y * DEST_WIDTH * 4
        for x in range(DEST_WIDTH):
            source = tiled_offset_2d(x, y)
            target = row + x * 4
            linear[target : target + 4] = tiled[source : source + 4]
    return bytes(linear)


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def compare(normal: bytes, observed_tiled: bytes) -> dict[str, Any]:
    if len(observed_tiled) != DEST_TILED_BYTES:
        raise ValueError(
            f"observed tiled extent is {len(observed_tiled)}, expected {DEST_TILED_BYTES}"
        )
    expected_linear = build_linear_oracle(normal)
    expected_tiled = tile_linear(expected_linear)
    mismatch = next(
        (index for index, pair in enumerate(zip(expected_tiled, observed_tiled)) if pair[0] != pair[1]),
        None,
    )
    observed_linear = untile(observed_tiled)
    nonzero_pixels = sum(
        any(observed_linear[offset : offset + 4])
        for offset in range(0, len(observed_linear), 4)
    )
    return {
        "schema": "ac6-demo-reached-copy-oracle-audit/v1",
        "match": mismatch is None,
        "first_mismatch_offset": mismatch,
        "normal_sha256": sha256(normal),
        "expected_linear_sha256": sha256(expected_linear),
        "expected_tiled_sha256": sha256(expected_tiled),
        "observed_linear_sha256": sha256(observed_linear),
        "observed_tiled_sha256": sha256(observed_tiled),
        "observed_nonzero_pixels": nonzero_pixels,
        "profile": {
            "source": "640x360 RGBA8",
            "destination": "1280x720 tiled raw-format-6",
            "scale": "2x nearest sample replication",
            "copy_dest_swap": "R/B",
            "padding_byte": f"0x{PADDING:02X}",
        },
    }


def self_test() -> dict[str, Any]:
    normal = bytes([0x11, 0x22, 0x33, 0x44]) * (NORMAL_WIDTH * NORMAL_HEIGHT)
    linear = build_linear_oracle(normal)
    tiled = tile_linear(linear)
    if sha256(linear) != EXPECTED_LINEAR_ASYMMETRIC:
        raise RuntimeError("linear asymmetric oracle digest changed")
    if sha256(tiled) != EXPECTED_TILED_ASYMMETRIC:
        raise RuntimeError("tiled asymmetric oracle digest changed")
    result = compare(normal, tiled)
    if not result["match"]:
        raise RuntimeError("self-test oracle rejected its own output")
    changed = bytearray(tiled)
    changed[tiled_offset_2d(321, 123)] ^= 1
    if compare(normal, bytes(changed))["match"]:
        raise RuntimeError("self-test failed to detect a changed pixel")
    return {
        "self_test": "PASS",
        "linear_sha256": sha256(linear),
        "tiled_sha256": sha256(tiled),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--normal-rgba", type=Path)
    parser.add_argument("--tiled-resolve", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        result: dict[str, Any] = self_test()
    else:
        if args.normal_rgba is None or args.tiled_resolve is None:
            parser.error("--normal-rgba and --tiled-resolve are required")
        result = compare(args.normal_rgba.read_bytes(), args.tiled_resolve.read_bytes())

    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json is not None:
        args.json.write_text(payload, encoding="utf-8")
    print(payload, end="")
    return 0 if result.get("match", True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
