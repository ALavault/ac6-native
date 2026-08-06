#!/usr/bin/env python3
"""Join bounded Vulkan gameplay vfetches to exact retail NDXR byte ranges."""

from __future__ import annotations

import argparse
import ctypes
import json
import re
import struct
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DRAW_RE = re.compile(
    r"\[ac6-gameplay-draw\] gpu_frame=(\d+) draw_ordinal=(\d+) .*?"
    r"vs=([0-9A-F]{16}) ps=([0-9A-F]{16}).*?"
    r"c218=([^ ]+) c219=([^ ]+) c220=([^ ]+) c221=([^ ]+)"
)
VFETCH_RE = re.compile(
    r"\[ac6-gameplay-vfetch\] gpu_frame=(\d+) draw_ordinal=(\d+) fc=(\d+) "
    r"raw=([0-9A-F]{8}),([0-9A-F]{8}) address=([0-9A-F]{8}) "
    r"bytes=(\d+) xxh3=([0-9A-F]{16}) head=([0-9A-F]{8}),([0-9A-F]{8})"
)
INDEX_RE = re.compile(
    r"\[ac6-gameplay-index\] gpu_frame=(\d+) draw_ordinal=(\d+) "
    r"address=([0-9A-F]{8}) bytes=(\d+) allocation_bytes=(\d+) count=(\d+) "
    r"format=(\d+) endian=(\d+) xxh3=([0-9A-F]{16}) "
    r"head=([0-9A-F]{8}),([0-9A-F]{8})"
)


@dataclass(frozen=True)
class Span:
    offset: int
    size: int
    path: str


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def read_fhm(data: bytes, span: Span) -> list[Span] | None:
    if data[span.offset : span.offset + 4] != b"FHM " or span.size < 12:
        return None
    order = data[span.offset + 5]
    endian = ">" if order == 1 else "<" if order == 0 else None
    if endian is None:
        return None
    directory = struct.unpack_from(endian + "H", data, span.offset + 6)[0]
    if directory + 4 > span.size:
        return None
    count = struct.unpack_from(endian + "I", data, span.offset + directory)[0]
    if count > 100_000 or directory + 4 + count * 16 > span.size:
        return None
    table = span.offset + directory + 4
    offsets = struct.unpack_from(endian + f"{count}I", data, table)
    sizes = struct.unpack_from(endian + f"{count}I", data, table + count * 4)
    children = []
    for index, (relative, size) in enumerate(zip(offsets, sizes)):
        if relative > span.size or size > span.size - relative:
            return None
        children.append(Span(span.offset + relative, size, f"{span.path}.{index}"))
    return children


def read_mdlp(data: bytes, span: Span) -> list[Span] | None:
    if data[span.offset : span.offset + 4] != b"MDLP" or span.size < 0x14:
        return None
    count, declared, table, payload = struct.unpack_from(">IIII", data, span.offset + 4)
    if declared > span.size or table + count * 4 > declared or payload > declared:
        return None
    offsets = struct.unpack_from(">" + f"{count}I", data, span.offset + table)
    children = []
    for index, relative in enumerate(offsets):
        begin = payload + relative
        end = declared if index + 1 == count else payload + offsets[index + 1]
        if end < begin or end > declared:
            return None
        children.append(Span(span.offset + begin, end - begin, f"{span.path}.m{index}"))
    return children


def walk_ndxr(data: bytes, span: Span) -> Iterable[Span]:
    children = read_fhm(data, span)
    if children is None:
        children = read_mdlp(data, span)
    if children is not None:
        for child in children:
            yield from walk_ndxr(data, child)
        return
    if data[span.offset : span.offset + 4] == b"NDXR" and be32(data, span.offset + 4) == span.size:
        yield span


def ndxr_names(data: bytes) -> tuple[list[str], list[tuple[int, int, int, int, int, int]]]:
    count = be16(data, 0x0A)
    header = be32(data, 0x10)
    region_one = be32(data, 0x14)
    region_two = be32(data, 0x18)
    additional = be32(data, 0x1C)
    name_base = 0x30 + header + region_one + region_two + additional
    names = []
    polygons = []
    polygon_descriptor = 0x30 + count * 0x30
    for object_index in range(count):
        descriptor = 0x30 + object_index * 0x30
        name_at = name_base + be32(data, descriptor + 0x20)
        name_end = data.find(b"\0", name_at)
        names.append(data[name_at:name_end].decode("ascii", "replace"))
        polygon_count = be16(data, descriptor + 0x2A)
        for _ in range(polygon_count):
            polygons.append(
                (
                    object_index,
                    be32(data, polygon_descriptor),
                    be32(data, polygon_descriptor + 4),
                    be32(data, polygon_descriptor + 8),
                    be16(data, polygon_descriptor + 0x0C),
                    be16(data, polygon_descriptor + 0x20),
                )
            )
            polygon_descriptor += 0x30
    return names, polygons


def xxh3_function():
    library = ctypes.CDLL("libxxhash.so.0")
    function = library.XXH3_64bits
    function.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    function.restype = ctypes.c_uint64

    def digest(data: bytes) -> int:
        if not data:
            return int(function(None, 0))
        buffer = ctypes.create_string_buffer(data, len(data))
        return int(function(buffer, len(data)))

    return digest


def candidate_ranges(source: str, path: str, data: bytes):
    header = be32(data, 0x10)
    polygon_size = be32(data, 0x14)
    vertex_size = be32(data, 0x18)
    additional_size = be32(data, 0x1C)
    polygon_base = 0x30 + header
    vertex_base = polygon_base + polygon_size
    additional_base = vertex_base + vertex_size
    names, polygons = ndxr_names(data)
    base_rows = [
        ("polygon_clump", polygon_base, polygon_size, None),
        ("vertex_clump", vertex_base, vertex_size, None),
        ("vertex_additional_clump", additional_base, additional_size, None),
    ]
    seen = set()
    for kind, offset, size, object_index in base_rows:
        if size and (offset, size) not in seen:
            seen.add((offset, size))
            yield source, path, kind, offset, data[offset : offset + size], names, object_index
    for (object_index, polygon_offset, vertex_offset, additional_offset,
         _vertex_count, index_count) in polygons:
        index_size = index_count * 2
        if index_size and polygon_offset <= polygon_size and index_size <= polygon_size - polygon_offset:
            offset = polygon_base + polygon_offset
            if (offset, index_size) not in seen:
                seen.add((offset, index_size))
                yield source, path, "index_range", offset, data[offset : offset + index_size], names, object_index
        if vertex_offset < vertex_size:
            offset = vertex_base + vertex_offset
            size = vertex_size - vertex_offset
            if (offset, size) not in seen:
                seen.add((offset, size))
                yield source, path, "vertex_suffix", offset, data[offset : offset + size], names, object_index
        if additional_size and additional_offset < additional_size:
            offset = additional_base + additional_offset
            size = additional_size - additional_offset
            if (offset, size) not in seen:
                seen.add((offset, size))
                yield source, path, "vertex_additional_suffix", offset, data[offset : offset + size], names, object_index


def inventory_entry9(decoded_path: Path):
    data = decoded_path.read_bytes()
    for span in walk_ndxr(data, Span(0, len(data), "root")):
        payload = data[span.offset : span.offset + span.size]
        yield from candidate_ranges("entry9", span.path, payload)


def inventory_entry119(root: Path):
    for path in sorted(root.rglob("*.ndxr")):
        data = path.read_bytes()
        if len(data) < 0x30 or data[:4] != b"NDXR" or be32(data, 4) != len(data):
            continue
        yield from candidate_ranges("entry119", str(path.relative_to(root)), data)


def parse_runtime(log_path: Path):
    draws = {}
    fetches = []
    indexes = []
    for line in log_path.read_text(errors="replace").splitlines():
        draw = DRAW_RE.search(line)
        if draw:
            frame, ordinal = map(int, draw.group(1, 2))
            draws[(frame, ordinal)] = {
                "frame": frame,
                "draw_ordinal": ordinal,
                "vertex_shader_xxh3": draw.group(3),
                "pixel_shader_xxh3": draw.group(4),
                "c218_c221": [group.split(",") for group in draw.group(5, 6, 7, 8)],
            }
            continue
        fetch = VFETCH_RE.search(line)
        if fetch:
            frame, ordinal, constant = map(int, fetch.group(1, 2, 3))
            fetches.append(
                {
                    "frame": frame,
                    "draw_ordinal": ordinal,
                    "fetch_constant": constant,
                    "raw": [fetch.group(4), fetch.group(5)],
                    "address": fetch.group(6),
                    "bytes": int(fetch.group(7)),
                    "xxh3": fetch.group(8),
                    "head": [fetch.group(9), fetch.group(10)],
                }
            )
            continue
        index = INDEX_RE.search(line)
        if index:
            frame, ordinal = map(int, index.group(1, 2))
            indexes.append(
                {
                    "frame": frame,
                    "draw_ordinal": ordinal,
                    "address": index.group(3),
                    "bytes": int(index.group(4)),
                    "allocation_bytes": int(index.group(5)),
                    "count": int(index.group(6)),
                    "format": int(index.group(7)),
                    "endian": int(index.group(8)),
                    "xxh3": index.group(9),
                    "head": [index.group(10), index.group(11)],
                }
            )
    return draws, fetches, indexes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--entry9", required=True, type=Path)
    parser.add_argument("--entry119-root", required=True, type=Path)
    args = parser.parse_args()

    digest = xxh3_function()
    index = defaultdict(list)
    candidate_count = 0
    for row in (*inventory_entry9(args.entry9), *inventory_entry119(args.entry119_root)):
        source, path, kind, offset, data, names, object_index = row
        candidate_count += 1
        key = (len(data), f"{digest(data):016X}")
        index[key].append(
            {
                "source": source,
                "path": path,
                "range_kind": kind,
                "range_offset": offset,
                "range_size": len(data),
                "object_index": object_index,
                "object_name": names[object_index] if object_index is not None else None,
                "object_names": names,
            }
        )

    draws, fetches, indexes = parse_runtime(args.log)
    matches = []
    unmatched = []
    for fetch in fetches:
        candidates = index.get((fetch["bytes"], fetch["xxh3"]), [])
        row = dict(fetch)
        row["draw"] = draws.get((fetch["frame"], fetch["draw_ordinal"]))
        if candidates:
            row["asset_matches"] = candidates
            matches.append(row)
        else:
            unmatched.append(row)

    unique_asset_matches = {}
    for row in matches:
        key = (row["frame"], row["draw_ordinal"], row["fetch_constant"], row["xxh3"])
        unique_asset_matches[str(key)] = row
    index_matches = []
    unmatched_indexes = []
    for runtime_index in indexes:
        candidates = index.get(
            (runtime_index["bytes"], runtime_index["xxh3"]), [])
        row = dict(runtime_index)
        row["draw"] = draws.get(
            (runtime_index["frame"], runtime_index["draw_ordinal"]))
        if candidates:
            row["asset_matches"] = candidates
            index_matches.append(row)
        else:
            unmatched_indexes.append(row)
    output = {
        "schema": "ac6.gameplay_frame_asset_join.v2",
        "runtime_log": str(args.log),
        "runtime": {
            "draw_records": len(draws),
            "vfetch_records": len(fetches),
            "matched_vfetch_records": len(matches),
            "unmatched_vfetch_records": len(unmatched),
            "unique_matched_fetches": len(unique_asset_matches),
            "index_records": len(indexes),
            "matched_index_records": len(index_matches),
            "unmatched_index_records": len(unmatched_indexes),
        },
        "offline": {
            "candidate_ranges": candidate_count,
            "entry9": str(args.entry9),
            "entry119_root": str(args.entry119_root),
        },
        "matches": list(unique_asset_matches.values()),
        "index_matches": index_matches,
    }
    print(json.dumps(output, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
