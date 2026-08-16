#!/usr/bin/env python3
"""Inventory embedded demo NSXR shader containers without publishing bytes."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import tempfile
from pathlib import Path

TARGET = "ac6-demo-xbox360-pal"
XEX_SHA256 = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
BASEFILE_SHA256 = "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218"
IMAGE_BASE = 0x82000000
FLAGS = {0x102A1100: "pixel", 0x102A1101: "vertex"}


class InventoryError(RuntimeError):
    pass


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def hx(value: int) -> str:
    return f"0x{value:08X}"


def inventory(basefile: bytes) -> dict[str, object]:
    wrappers: list[dict[str, object]] = []
    cursor = 0
    while True:
        start = basefile.find(b"NSXR", cursor)
        if start < 0:
            break
        cursor = start + 4
        if start + 16 > len(basefile):
            continue
        size = u32(basefile, start + 4)
        if size < 0x40 or size > 0x100000 or start + size > len(basefile):
            continue
        containers: list[dict[str, object]] = []
        for relative in range(0x20, size - 0x24 + 1, 0x20):
            absolute = start + relative
            flags = u32(basefile, absolute)
            if flags not in FLAGS:
                continue
            virtual_size = u32(basefile, absolute + 4)
            physical_size = u32(basefile, absolute + 8)
            shader_offset = u32(basefile, absolute + 0x18)
            total = virtual_size + physical_size
            if virtual_size < 0x24 or total > size - relative or shader_offset + 8 > virtual_size:
                continue
            physical_offset = u32(basefile, absolute + shader_offset)
            shader_size = u32(basefile, absolute + shader_offset + 4)
            code_offset = virtual_size + physical_offset
            if not shader_size or shader_size % 4 or code_offset + shader_size > total:
                continue
            container = basefile[absolute:absolute + total]
            microcode = container[code_offset:code_offset + shader_size]
            containers.append({
                "address": hx(IMAGE_BASE + absolute),
                "wrapper_offset": hx(relative),
                "stage": FLAGS[flags],
                "flags": hx(flags),
                "virtual_size": virtual_size,
                "physical_size": physical_size,
                "container_size": total,
                "container_sha256": digest(container),
                "shader_offset": hx(shader_offset),
                "microcode_offset": hx(code_offset),
                "microcode_size": shader_size,
                "microcode_sha256": digest(microcode),
            })
        if containers:
            wrappers.append({
                "address": hx(IMAGE_BASE + start),
                "size": size,
                "byte_sha256": digest(basefile[start:start + size]),
                "containers": containers,
            })
    return {
        "schema": "ac6-demo-nsxr-shader-inventory/v1",
        "target_id": TARGET,
        "xex_sha256": XEX_SHA256,
        "basefile_sha256": digest(basefile),
        "byte_order": "big-endian",
        "proprietary_bytes_published": False,
        "wrapper_count": len(wrappers),
        "container_count": sum(len(w["containers"]) for w in wrappers),
        "wrappers": wrappers,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xex", type=Path, required=True)
    parser.add_argument("--basefile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if digest(args.xex.read_bytes()) != XEX_SHA256:
        raise InventoryError("Default.xex identity mismatch")
    basefile = args.basefile.read_bytes()
    if digest(basefile) != BASEFILE_SHA256:
        raise InventoryError("basefile identity mismatch")
    encoded = (json.dumps(inventory(basefile), indent=2, sort_keys=True) + "\n").encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix=args.output.name + ".",
                                     delete=False) as temporary:
        temporary.write(encoded)
        temporary.flush()
        os.fsync(temporary.fileno())
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
