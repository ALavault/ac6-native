#!/usr/bin/env python3
"""Fail-closed analysis of the reached PAL demo rectangle vertex shader."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import tempfile
from pathlib import Path

XEX = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
SHADER = "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b"
VERTICES = "cf61dc45a42c755414326bbafa7d69dedc9046eef9b4d70a4326f6ea1dac7db1"


class AnalysisError(RuntimeError):
    pass


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cf_pair(words: list[int], offset: int) -> tuple[int, int]:
    a, b, c = words[offset:offset + 3]
    return a | ((b & 0xFFFF) << 32), (b >> 16) | (c << 16)


def cf_fields(raw: int) -> tuple[int, int, int, int]:
    return (raw >> 44) & 0xF, raw & 0xFFF, (raw >> 12) & 7, (raw >> 16) & 0xFFF


def fetch(words: list[int], offset: int) -> dict[str, object]:
    a, b, c = words[offset:offset + 3]
    signed_offset = (c >> 8) & 0x7FFFFF
    if signed_offset & 0x400000:
        signed_offset -= 0x800000
    return {
        "opcode": a & 0x1F, "src_register": (a >> 5) & 0x3F,
        "dst_register": (a >> 12) & 0x3F,
        "constant_index": (a >> 20) & 0x1F,
        "constant_index_select": (a >> 25) & 3,
        "dst_swizzle": [(b >> (3 * i)) & 7 for i in range(4)],
        "format": (b >> 16) & 0x3F, "mini_fetch": bool((b >> 30) & 1),
        "stride_dwords": c & 0xFF, "offset_dwords": signed_offset,
    }


def alu(words: list[int], offset: int) -> dict[str, object]:
    a, _, c = words[offset:offset + 3]
    return {
        "vector_dest": a & 0x3F, "export": bool((a >> 15) & 1),
        "vector_write_mask": (a >> 16) & 0xF,
        "vector_opcode": (c >> 24) & 0x1F,
        "src1_register": (c >> 16) & 0xFF,
        "src2_register": (c >> 8) & 0xFF,
        "src1_is_register": bool((c >> 31) & 1),
        "src2_is_register": bool((c >> 30) & 1),
    }


def analyze(report: dict[str, object], report_sha256: str,
            vertex_bytes: bytes) -> dict[str, object]:
    target = report.get("target", {})
    if target.get("xex_sha256") != XEX:
        raise AnalysisError("report identity mismatch")
    buffers = report.get("graphics", {}).get("ring", {}).get("indirect_buffers", [])
    main = next((item for item in buffers if item.get("dword_count") == 3029), None)
    if main is None or main.get("byte_sha256") != (
            "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6"):
        raise AnalysisError("main IB identity mismatch")
    words = [int(value, 0) if isinstance(value, str) else int(value)
             for value in main["dwords"]]
    header = words[146]
    count = ((header >> 16) & 0x3FFF) + 1
    if (header >> 30, (header >> 8) & 0x7F, count) != (3, 0x2B, 29):
        raise AnalysisError("reached shader packet mismatch")
    code = words[149:176]
    code_bytes = b"".join(struct.pack(">I", word) for word in code)
    if sha(code_bytes) != SHADER:
        raise AnalysisError("vertex microcode mismatch")
    control = [cf_fields(raw) for pair_offset in (0, 3, 6)
               for raw in cf_pair(code, pair_offset)]
    if control != [(1, 3, 2, 5), (12, 0, 0, 0), (1, 5, 1, 0),
                   (12, 0, 0, 0), (1, 6, 1, 0), (2, 7, 1, 0)]:
        raise AnalysisError("vertex control flow mismatch")
    position_fetch, color_fetch = fetch(code, 9), fetch(code, 12)
    color_export, position_export = alu(code, 15), alu(code, 18)
    expected = (
        (position_fetch["opcode"], position_fetch["dst_register"],
         position_fetch["format"], position_fetch["stride_dwords"],
         position_fetch["offset_dwords"], position_fetch["dst_swizzle"]),
        (color_fetch["opcode"], color_fetch["dst_register"], color_fetch["format"],
         color_fetch["offset_dwords"], color_fetch["mini_fetch"],
         color_fetch["dst_swizzle"]),
        (color_export["export"], color_export["vector_dest"],
         color_export["vector_opcode"], color_export["src1_register"],
         color_export["src2_register"], color_export["vector_write_mask"]),
        (position_export["export"], position_export["vector_dest"],
         position_export["vector_opcode"], position_export["src1_register"],
         position_export["src2_register"], position_export["vector_write_mask"]),
    )
    if expected != ((0, 1, 57, 7, 0, [0, 1, 2, 5]),
                    (0, 0, 38, 3, True, [0, 1, 2, 3]),
                    (True, 0, 2, 0, 0, 15),
                    (True, 62, 2, 1, 1, 15)):
        raise AnalysisError("vertex data flow mismatch")
    if len(vertex_bytes) != 84 or sha(vertex_bytes) != VERTICES:
        raise AnalysisError("vertex capsule mismatch")
    values = list(struct.unpack(">21f", vertex_bytes))
    records = [values[index:index + 7] for index in range(0, 21, 7)]
    if any(record[3:] != [0.0, 0.0, 0.0, 0.0] for record in records):
        raise AnalysisError("nonzero reached vertex color")
    return {
        "schema": "ac6-demo-reached-vertex-shader-analysis/v1",
        "target_id": "ac6-demo-xbox360-pal", "xex_sha256": XEX,
        "source_report_sha256": report_sha256,
        "microcode_sha256": SHADER, "microcode_bytes_published": False,
        "vertex_capsule_sha256": VERTICES, "vertex_bytes_published": False,
        "xenia_generic": {"vertex_formats": {"57": "k_32_32_32_FLOAT",
                                               "38": "k_32_32_32_32_FLOAT"},
                          "vector_opcode_2": "max", "export_position": 62},
        "demo_qualified": {
            "position_fetch": position_fetch, "color_fetch": color_fetch,
            "color_export": "interpolator0=max(r0,r0)",
            "position_export": "position=max(r1,r1)",
            "positions": [record[:3] for record in records],
            "vertex_colors": [record[3:] for record in records],
            "pixel_input_color0": [0.0, 0.0, 0.0, 0.0],
        },
        "unknown": ["EDRAM content outside the reached rectangle",
                    "effects of earlier bootstrap point draws"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--vertex-bytes-hex", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report_bytes = args.report.read_bytes()
    result = analyze(json.loads(report_bytes), sha(report_bytes),
                     bytes.fromhex(args.vertex_bytes_hex))
    encoded = (json.dumps(result, indent=2, sort_keys=True) + "\n").encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{args.output.name}.", dir=args.output.parent)
    with os.fdopen(fd, "wb") as stream:
        stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
