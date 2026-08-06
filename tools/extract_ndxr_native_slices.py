#!/usr/bin/env python3
"""Extract bounded binary NDXR slices and native contracts from decoded data.

This is an offline indexer. It never opens a PAC archive and writes only the
selected NDXR records plus TSV contracts consumed by the native loader.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import struct
import sys
from pathlib import Path


def load_joiner() -> object:
    source = Path(__file__).with_name("ac6_join_gameplay_frame_assets.py")
    spec = importlib.util.spec_from_file_location("ac6_joiner", source)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load NDXR walker")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def contract(data: bytes) -> tuple[int, int, int, int]:
    if data[:4] != b"NDXR" or be32(data, 4) != len(data):
        raise ValueError("invalid NDXR slice")
    object_count = be16(data, 0x0A)
    header, polygon_size, vertex_size, _ = struct.unpack_from(">IIII", data, 0x10)
    descriptor = 0x30 + object_count * 0x30
    polygon_count = sum(be16(data, 0x30 + i * 0x30 + 0x2A) for i in range(object_count))
    descriptor_end = descriptor + polygon_count * 0x30
    polygon_base = 0x30 + header
    vertex_base = polygon_base + polygon_size
    if descriptor_end > polygon_base or vertex_base + vertex_size > len(data):
        raise ValueError("NDXR section bounds are invalid")
    stride = 0
    max_vertex_end = 0
    index_count = 0
    for i in range(polygon_count):
        offset = descriptor + i * 0x30
        vertex_count = be16(data, offset + 0x0C)
        fmt = be16(data, offset + 0x0E)
        candidate = {0x0611: 28, 0x0613: 32, 0x0711: 44, 0x0721: 52}.get(fmt, 0)
        if candidate == 0 or (stride and stride != candidate):
            raise ValueError(f"unsupported NDXR vertex format 0x{fmt:02x}")
        stride = candidate
        vertex_offset = be32(data, offset + 4)
        index_offset = be32(data, offset)
        indices = be16(data, offset + 0x20)
        if vertex_offset % stride or vertex_offset + vertex_count * stride > vertex_size or index_offset + indices * 2 > polygon_size:
            raise ValueError("NDXR polygon stream exceeds its section")
        max_vertex_end = max(max_vertex_end, vertex_offset + vertex_count * stride)
        index_count += indices
    if not stride or max_vertex_end == 0 or (max_vertex_end + stride - 1) // stride > 0xFFFFFFFF:
        raise ValueError("NDXR vertex stream is not stride-aligned")
    return (max_vertex_end + stride - 1) // stride, index_count, polygon_count, stride


def fnv64(data: bytes) -> int:
    value = 1469598103934665603
    for byte in data:
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("decoded_entry", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--names", default="f16|010_NDXR|144_NDXR")
    args = parser.parse_args()
    joiner = load_joiner()
    data = args.decoded_entry.read_bytes() if args.decoded_entry.is_file() else None
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    selected = []
    spans = []
    if data is not None:
        spans = [("root", data)]
        walked = joiner.walk_ndxr(data, joiner.Span(0, len(data), "root"))
        spans = [(span.path, data[span.offset : span.offset + span.size]) for span in walked]
    elif args.decoded_entry.is_dir():
        spans = [(str(path.relative_to(args.decoded_entry)), path.read_bytes())
                 for path in sorted(args.decoded_entry.rglob("*.ndxr"))]
    else:
        raise SystemExit("error: decoded entry must be a file or NDXR directory")
    for span_name, payload in spans:
        try:
            names, _ = joiner.ndxr_names(payload)
            vertex_count, index_count, primitive_count, stride = contract(payload)
        except (IndexError, struct.error, ValueError):
            continue
        identity = " ".join(names).lower() + " " + span_name.lower()
        if not __import__("re").search(args.names.lower(), identity):
            continue
        safe = span_name.replace(".", "_").replace("/", "_") + ".ndxr"
        path = output / safe
        path.write_bytes(payload)
        sha = hashlib.sha256(payload).hexdigest()
        buffer_id = safe.removesuffix(".ndxr")
        selected.append((buffer_id, path.name, len(payload), fnv64(payload), sha,
                         vertex_count, index_count, primitive_count, stride, names))
    if not selected:
        raise SystemExit("error: no qualified NDXR matched")
    with (output / "native-buffers.tsv").open("w") as buffers, \
         (output / "native-drawables.tsv").open("w") as drawables:
        buffers.write("# buffer_id path byte_size fnv64\n")
        drawables.write("# mission stable_id kind asset primitive buffer vertex_count index_count content_hash\n")
        for buffer_id, name, size, digest, sha, vertices, indices, primitives, stride, names in selected:
            buffers.write(f"{buffer_id}\t{name}\t{size}\t{digest}\n")
            joined = " ".join(names).lower()
            asset = 9 if "f16" in joined else 119
            kind = "aircraft" if asset == 9 else "terrain"
            stable = names[0] if names else buffer_id
            drawables.write(f"1\t{stable}\t{kind}\t{asset}\t{primitives}\t{buffer_id}\t"
                            f"{vertices}\t{indices}\t{sha}\n")
    print(f"slices={len(selected)}")
    print(f"output={output}")
    for row in selected:
        print(f"{row[0]} vertices={row[5]} indices={row[6]} primitives={row[7]} stride={row[8]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
