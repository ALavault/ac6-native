#!/usr/bin/env python3
"""Extract bounded NTXR files and emit identity metadata for native manifests."""
from __future__ import annotations

import argparse
import hashlib
import re
import shutil
from pathlib import Path


def be16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "big")


def be32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "big")


def inspect(data: bytes) -> tuple[int, int, int]:
    if len(data) < 0x28 or data[:4] != b"NTXR":
        raise ValueError("not NTXR")
    width, height = be16(data, 0x24), be16(data, 0x26)
    if not width or not height or width > 16384 or height > 16384:
        raise ValueError("invalid NTXR dimensions")
    return width, height, be16(data, 0x04)


def fnv64(data: bytes) -> int:
    value = 1469598103934665603
    for byte in data:
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--names", default="NTXR")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for source in sorted(args.input_dir.rglob("*.ntxr")):
        if not re.search(args.names, str(source), re.IGNORECASE):
            continue
        data = source.read_bytes()
        try:
            width, height, fmt = inspect(data)
        except ValueError:
            continue
        name = source.relative_to(args.input_dir).as_posix().replace("/", "_")
        destination = output / name
        shutil.copyfile(source, destination)
        digest = fnv64(data)
        rows.append((name, destination.stat().st_size, digest, hashlib.sha256(data).hexdigest(),
                     width, height, fmt))
    if not rows:
        raise SystemExit("error: no qualified NTXR matched")
    with (output / "native-textures.tsv").open("w") as stream:
        stream.write("# texture_id path byte_size fnv64 sha256 width height format\n")
        for name, size, digest, sha, width, height, fmt in rows:
            stream.write(f"{name}\t{name}\t{size}\t{digest}\t{sha}\t{width}\t{height}\t{fmt}\n")
    print(f"textures={len(rows)}")
    print(f"output={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
