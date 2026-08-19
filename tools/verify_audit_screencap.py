#!/usr/bin/env python3
"""Verify an AC6 renderer audit PNG and its optional JSON sidecar."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def parse_png(path: Path) -> dict:
    payload = path.read_bytes()
    if payload[:8] != PNG_SIGNATURE:
        raise ValueError("PNG signature mismatch")
    offset = 8
    width = height = None
    bit_depth = color_type = compression = filtering = interlace = None
    idat = bytearray()
    text: dict[str, str] = {}
    saw_iend = False
    while offset < len(payload):
        if offset + 12 > len(payload):
            raise ValueError("truncated PNG chunk header")
        length = struct.unpack_from(">I", payload, offset)[0]
        chunk_type = payload[offset + 4:offset + 8]
        end = offset + 12 + length
        if end > len(payload):
            raise ValueError("truncated PNG chunk payload")
        chunk = payload[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack_from(">I", payload, offset + 8 + length)[0]
        actual_crc = zlib.crc32(chunk_type + chunk) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError(f"CRC mismatch for {chunk_type!r}")
        if chunk_type == b"IHDR":
            if length != 13 or width is not None:
                raise ValueError("invalid or duplicate IHDR")
            width, height, bit_depth, color_type, compression, filtering, interlace = \
                struct.unpack(">IIBBBBB", chunk)
        elif chunk_type == b"tEXt":
            if b"\0" not in chunk:
                raise ValueError("invalid tEXt chunk")
            key, value = chunk.split(b"\0", 1)
            text[key.decode("latin-1")] = value.decode("latin-1")
        elif chunk_type == b"IDAT":
            idat += chunk
        elif chunk_type == b"IEND":
            if length != 0:
                raise ValueError("IEND is not empty")
            saw_iend = True
        offset = end
    if offset != len(payload) or not saw_iend or width is None or not idat:
        raise ValueError("incomplete PNG")
    if (bit_depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
        raise ValueError("PNG is not the qualified non-interlaced RGBA8 profile")
    raw = zlib.decompress(bytes(idat))
    row_bytes = width * 4
    if len(raw) != height * (row_bytes + 1):
        raise ValueError("decompressed PNG extent mismatch")
    rows = []
    for y in range(height):
        row = raw[y * (row_bytes + 1):(y + 1) * (row_bytes + 1)]
        if row[0] != 0:
            raise ValueError("audit PNG uses an unexpected row filter")
        rows.append(row[1:])
    rgba = b"".join(rows)
    rgb_nonzero = 0
    rgba_zero = 0
    alpha_zero = alpha_full = alpha_partial = 0
    for offset in range(0, len(rgba), 4):
        r, g, b, a = rgba[offset:offset + 4]
        rgb_nonzero += int(bool(r or g or b))
        rgba_zero += int(not (r or g or b or a))
        alpha_zero += int(a == 0)
        alpha_full += int(a == 255)
        alpha_partial += int(a not in (0, 255))
    return {
        "path": str(path),
        "png_sha256": hashlib.sha256(payload).hexdigest(),
        "rgba8_sha256": hashlib.sha256(rgba).hexdigest(),
        "width": width,
        "height": height,
        "pixels": width * height,
        "rgb_nonzero": rgb_nonzero,
        "rgba_zero": rgba_zero,
        "alpha_zero": alpha_zero,
        "alpha_full": alpha_full,
        "alpha_partial": alpha_partial,
        "rgb_all_black": rgb_nonzero == 0,
        "text": text,
    }


def verify_sidecar(summary: dict, sidecar: Path) -> None:
    document = json.loads(sidecar.read_text())
    if document.get("schema") != "ac6-demo-renderer-audit-screencap/v1":
        raise ValueError("sidecar schema mismatch")
    capture = document.get("capture", {})
    pixels = document.get("pixels", {})
    expected = {
        "png": Path(summary["path"]).name,
        "png_sha256": summary["png_sha256"],
        "rgba8_sha256": summary["rgba8_sha256"],
        "width": summary["width"],
        "height": summary["height"],
    }
    for key, value in expected.items():
        if capture.get(key) != value:
            raise ValueError(f"sidecar capture field mismatch: {key}")
    for key in ("count", "rgb_nonzero", "rgba_zero", "alpha_zero",
                "alpha_full", "alpha_partial", "rgb_all_black"):
        summary_key = "pixels" if key == "count" else key
        if pixels.get(key) != summary[summary_key]:
            raise ValueError(f"sidecar pixel field mismatch: {key}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("png", type=Path)
    parser.add_argument("--sidecar", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    summary = parse_png(args.png)
    sidecar = args.sidecar
    if sidecar is None:
        candidate = args.png.with_suffix(".json")
        if candidate.is_file():
            sidecar = candidate
    if sidecar is not None:
        verify_sidecar(summary, sidecar)
        summary["sidecar"] = str(sidecar)
        summary["sidecar_verified"] = True
    else:
        summary["sidecar_verified"] = False
    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print(
            "audit_screencap=PASS "
            f"width={summary['width']} height={summary['height']} "
            f"png_sha256={summary['png_sha256']} "
            f"rgba8_sha256={summary['rgba8_sha256']} "
            f"rgb_nonzero={summary['rgb_nonzero']} "
            f"rgb_all_black={int(summary['rgb_all_black'])} "
            f"sidecar={int(summary['sidecar_verified'])}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
