#!/usr/bin/env python3
"""Localize divergence in the reached AC6 PAL EDRAM/copy/tiling path.

Inputs are caller-provided runtime artifacts. The tool never modifies game data.
It independently rebuilds:
- the 0xA00000-byte EDRAM audit allocation from a 640x360 RGBA8 readback;
- the 1280x720 copy/convert result with the qualified R/B swap;
- the 0x398000-byte Xenos tiled destination with 0xA5 audit padding.
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
EDRAM_BYTES = 0xA00000
EDRAM_SURFACE_BYTES = 0x384000
EDRAM_TILE_WIDTH = 80
EDRAM_TILE_HEIGHT = 16
EDRAM_PITCH_TILES = 16
EDRAM_TILE_BYTES = EDRAM_TILE_WIDTH * EDRAM_TILE_HEIGHT * 4
EDRAM_CANARY = 0x5A
PADDING_CANARY = 0xA5


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


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


def edram_offset(sample_x: int, sample_y: int) -> int:
    if not (0 <= sample_x < DEST_WIDTH and 0 <= sample_y < DEST_HEIGHT):
        raise ValueError("EDRAM sample coordinate outside reached surface")
    tile_x, in_x = divmod(sample_x, EDRAM_TILE_WIDTH)
    tile_y, in_y = divmod(sample_y, EDRAM_TILE_HEIGHT)
    result = (
        (tile_y * EDRAM_PITCH_TILES + tile_x) * EDRAM_TILE_BYTES
        + (in_y * EDRAM_TILE_WIDTH + in_x) * 4
    )
    if result < 0 or result + 4 > EDRAM_SURFACE_BYTES:
        raise ValueError("EDRAM sample offset outside reached surface")
    return result


def materialize_edram(normal: bytes) -> bytes:
    if len(normal) != NORMAL_BYTES:
        raise ValueError(f"normal extent is {len(normal)}, expected {NORMAL_BYTES}")
    edram = bytearray([EDRAM_CANARY]) * EDRAM_BYTES
    for y in range(NORMAL_HEIGHT):
        source_row = y * NORMAL_WIDTH * 4
        for x in range(NORMAL_WIDTH):
            source = source_row + x * 4
            pixel = normal[source : source + 4]
            for sample_y in range(2):
                for sample_x in range(2):
                    target = edram_offset(2 * x + sample_x, 2 * y + sample_y)
                    edram[target : target + 4] = pixel
    return bytes(edram)


def build_linear(normal: bytes) -> bytes:
    if len(normal) != NORMAL_BYTES:
        raise ValueError(f"normal extent is {len(normal)}, expected {NORMAL_BYTES}")
    destination = bytearray(DEST_LINEAR_BYTES)
    for y in range(DEST_HEIGHT):
        source_row = (y >> 1) * NORMAL_WIDTH * 4
        destination_row = y * DEST_WIDTH * 4
        for x in range(DEST_WIDTH):
            source = source_row + (x >> 1) * 4
            target = destination_row + x * 4
            destination[target : target + 4] = bytes(
                (normal[source + 2], normal[source + 1],
                 normal[source + 0], normal[source + 3])
            )
    return bytes(destination)


def tile(linear: bytes) -> tuple[bytes, bytearray]:
    if len(linear) != DEST_LINEAR_BYTES:
        raise ValueError("linear destination extent is invalid")
    tiled = bytearray([PADDING_CANARY]) * DEST_TILED_BYTES
    addressed = bytearray(DEST_TILED_BYTES)
    for y in range(DEST_HEIGHT):
        row = y * DEST_WIDTH * 4
        for x in range(DEST_WIDTH):
            source = row + x * 4
            target = tiled_offset_2d(x, y)
            tiled[target : target + 4] = linear[source : source + 4]
            addressed[target : target + 4] = b"\x01\x01\x01\x01"
    return bytes(tiled), addressed


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


def first_byte_difference(expected: bytes, observed: bytes) -> tuple[int | None, int, int]:
    for offset, (left, right) in enumerate(zip(expected, observed)):
        if left != right:
            return offset, left, right
    return None, 0, 0


def diagnose(normal: bytes, observed_tiled: bytes,
             observed_edram: bytes | None = None) -> dict[str, Any]:
    if len(observed_tiled) != DEST_TILED_BYTES:
        raise ValueError(
            f"observed tiled extent is {len(observed_tiled)}, expected {DEST_TILED_BYTES}"
        )
    if observed_edram is not None and len(observed_edram) != EDRAM_BYTES:
        raise ValueError(
            f"observed EDRAM extent is {len(observed_edram)}, expected {EDRAM_BYTES}"
        )

    expected_linear = build_linear(normal)
    expected_tiled, addressed = tile(expected_linear)
    observed_linear = untile(observed_tiled)

    first_pixel: dict[str, int] | None = None
    pixel_mismatched_bytes = 0
    pixel_mismatched_pixels = 0
    for pixel in range(DEST_WIDTH * DEST_HEIGHT):
        pixel_differs = False
        for channel in range(4):
            offset = pixel * 4 + channel
            expected, observed = expected_linear[offset], observed_linear[offset]
            if expected == observed:
                continue
            pixel_differs = True
            pixel_mismatched_bytes += 1
            if first_pixel is None:
                first_pixel = {
                    "x": pixel % DEST_WIDTH,
                    "y": pixel // DEST_WIDTH,
                    "channel": channel,
                    "expected": expected,
                    "observed": observed,
                }
        pixel_mismatched_pixels += int(pixel_differs)

    first_padding: dict[str, int] | None = None
    padding_mismatched_bytes = 0
    for offset, used in enumerate(addressed):
        if used or observed_tiled[offset] == expected_tiled[offset]:
            continue
        padding_mismatched_bytes += 1
        if first_padding is None:
            first_padding = {
                "offset": offset,
                "expected": expected_tiled[offset],
                "observed": observed_tiled[offset],
            }

    edram_result: dict[str, Any] = {"provided": observed_edram is not None}
    edram_exact = True
    if observed_edram is not None:
        expected_edram = materialize_edram(normal)
        first_offset, expected, observed = first_byte_difference(
            expected_edram, observed_edram
        )
        mismatches = sum(left != right for left, right in zip(expected_edram, observed_edram))
        edram_exact = mismatches == 0
        edram_result.update(
            {
                "exact": edram_exact,
                "mismatched_bytes": mismatches,
                "expected_sha256": sha256(expected_edram),
                "observed_sha256": sha256(observed_edram),
                "first_difference": None
                if first_offset is None
                else {
                    "offset": first_offset,
                    "expected": expected,
                    "observed": observed,
                },
            }
        )

    pixels_exact = pixel_mismatched_bytes == 0
    padding_exact = padding_mismatched_bytes == 0
    if not edram_exact:
        first_failed_stage = "edram_materialization"
    elif not pixels_exact:
        first_failed_stage = "copy_pixels"
    elif not padding_exact:
        first_failed_stage = "destination_padding"
    else:
        first_failed_stage = "exact"

    return {
        "schema": "ac6-demo-reached-copy-differential/v1",
        "exact": first_failed_stage == "exact",
        "first_failed_stage": first_failed_stage,
        "normal_sha256": sha256(normal),
        "edram": edram_result,
        "copy_pixels": {
            "exact": pixels_exact,
            "mismatched_bytes": pixel_mismatched_bytes,
            "mismatched_pixels": pixel_mismatched_pixels,
            "first_difference": first_pixel,
            "expected_linear_sha256": sha256(expected_linear),
            "observed_linear_sha256": sha256(observed_linear),
        },
        "destination": {
            "tiled_exact": expected_tiled == observed_tiled,
            "padding_exact": padding_exact,
            "padding_mismatched_bytes": padding_mismatched_bytes,
            "first_padding_difference": first_padding,
            "expected_tiled_sha256": sha256(expected_tiled),
            "observed_tiled_sha256": sha256(observed_tiled),
        },
    }


def spatial_normal() -> bytes:
    result = bytearray(NORMAL_BYTES)
    for y in range(NORMAL_HEIGHT):
        for x in range(NORMAL_WIDTH):
            offset = (y * NORMAL_WIDTH + x) * 4
            result[offset + 0] = (x ^ (3 * y)) & 0xFF
            result[offset + 1] = (5 * x + y) & 0xFF
            result[offset + 2] = ((x >> 2) ^ (y >> 1)) & 0xFF
            result[offset + 3] = 0x80 | ((x + y) & 0x7F)
    return bytes(result)


def self_test() -> dict[str, Any]:
    # One full-size spatial pass is intentionally enough here. The C++ suite
    # exercises every mismatch class under ASan/UBSan; repeating four complete
    # 1280x720 Python diagnoses would turn a self-test into a small geological era.
    normal = spatial_normal()
    expected_linear = build_linear(normal)
    expected_tiled, addressed = tile(expected_linear)
    expected_edram = materialize_edram(normal)
    exact = diagnose(normal, expected_tiled, expected_edram)
    if not exact["exact"]:
        raise RuntimeError("exact self-test was rejected")

    pixel_offset = tiled_offset_2d(901, 447) + 1
    if not addressed[pixel_offset]:
        raise RuntimeError("pixel probe is not marked as addressed")
    padding_offset = addressed.index(0)
    if expected_tiled[padding_offset] != PADDING_CANARY:
        raise RuntimeError("padding probe does not contain the audit canary")
    edram_probe = edram_offset(42, 17) + 2
    if expected_edram[edram_probe] == EDRAM_CANARY:
        raise RuntimeError("EDRAM probe unexpectedly points into canary space")

    return {
        "schema": "ac6-demo-reached-copy-differential-self-test/v1",
        "self_test": "PASS",
        "normal_sha256": sha256(normal),
        "expected_linear_sha256": sha256(expected_linear),
        "expected_tiled_sha256": sha256(expected_tiled),
        "expected_edram_sha256": sha256(expected_edram),
        "pixel_probe_offset": pixel_offset,
        "padding_probe_offset": padding_offset,
        "edram_probe_offset": edram_probe,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--normal-rgba", type=Path)
    parser.add_argument("--tiled-resolve", type=Path)
    parser.add_argument("--edram", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        result: dict[str, Any] = self_test()
    else:
        if args.normal_rgba is None or args.tiled_resolve is None:
            parser.error("--normal-rgba and --tiled-resolve are required")
        result = diagnose(
            args.normal_rgba.read_bytes(),
            args.tiled_resolve.read_bytes(),
            args.edram.read_bytes() if args.edram is not None else None,
        )

    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json is not None:
        args.json.write_text(payload, encoding="utf-8")
    print(payload, end="")
    return 0 if result.get("exact", True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
