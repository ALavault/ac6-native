#!/usr/bin/env python3
"""Verify the qualified demo NDXR material keys and NSXR shader pairs."""

import argparse
import csv
import struct
from collections import Counter, defaultdict
from pathlib import Path


def be16(data, offset):
    return struct.unpack_from(">H", data, offset)[0]


def be32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def shader_records(data, begin, end):
    records = []
    for offset in range((begin + 0x7F) & ~0x7F, min(end, len(data)), 0x80):
        magic = data[offset:offset + 4]
        if magic not in (b"\x10\x2a\x11\x00", b"\x10\x2a\x11\x01"):
            continue
        length = be32(data, offset + 0x28)
        raw = data[offset + 0x2C:offset + 0x2C + length]
        updb = raw.rstrip(b"\0").decode("ascii", "replace")
        records.append(("PS" if magic[-1] == 0 else "VS", offset,
                        updb.rsplit("\\", 1)[-1]))
    return records


def nsxr_pairs(root):
    pairs = defaultdict(list)
    descriptor_count = 0
    inventory = list(csv.DictReader(
        (root / "fhm-resource-inventory.tsv").open(), delimiter="\t"))
    paths = source_paths(root)
    for row in inventory:
        if row["magic_hex"] != "4e535852":
            continue
        data = leaf(paths[row["source"]], row)
        label = f"{row['source']}:{row['tree']}"
        offset = 0x20
        for index in range(be16(data, 0x0A)):
            if offset + 0x1C > len(data):
                raise ValueError(f"{label}: descriptor {index} is out of bounds")
            key = be32(data, offset)
            size = be32(data, offset + 0x18)
            if size == 0 or offset + size > len(data):
                raise ValueError(f"{label}: invalid descriptor size at 0x{offset:X}")
            records = shader_records(data, offset, offset + size)
            pairs[key].append((label, offset, records))
            descriptor_count += 1
            offset += size
    return pairs, descriptor_count


def source_paths(root):
    paths = {}
    for dirname in ("pac-codec1-decompressed", "pac-codec2-decrypted"):
        for path in (root / dirname).glob("*.fhm"):
            paths[path.name] = path
    return paths


def leaf(path, row):
    with path.open("rb") as stream:
        stream.seek(int(row["offset"], 16))
        return stream.read(int(row["size"]))


def material_parameters(data):
    base = data.find(b"mapparts_")
    if base < 0:
        return {}
    records = defaultdict(list)
    for offset in range(0, len(data) - 0x13, 4):
        if be32(data, offset) == 0x20 and be32(data, offset + 8) == 4:
            records[be32(data, offset + 4)].append(offset)
    result = {}
    for name in ("NU_HASH", "NU_FLAG1", "NU_FLAG2"):
        string_offset = data.find(name.encode() + b"\0", base)
        if string_offset < 0:
            continue
        for offset in records.get(string_offset - base, ()):
            if offset < base:
                result[name] = be32(data, offset + 0x10)
                break
    return result


def ndxr_census(root):
    inventory = list(csv.DictReader(
        (root / "fhm-resource-inventory.tsv").open(), delimiter="\t"))
    paths = source_paths(root)
    target_sources = {row["source"] for row in inventory
                      if "mapparts_" in row["strings"]}
    keys = Counter()
    formats = Counter()
    flags = Counter()
    gids = set()
    objects = descriptors = material_slots = 0
    for row in inventory:
        if (row["source"] not in target_sources or
                row["magic_hex"] != "4e445852"):
            continue
        data = leaf(paths[row["source"]], row)
        if b"mapparts_" not in data:
            continue
        objects += 1
        params = material_parameters(data)
        flags[(params.get("NU_FLAG1"), params.get("NU_FLAG2"))] += 1
        seen_materials = set()
        for record_index in range(be16(data, 0x0A)):
            record = 0x30 + record_index * 0x30
            count = be16(data, record + 0x2A)
            descriptor = be32(data, record + 0x2C)
            for index in range(count):
                item = descriptor + index * 0x30
                if item + 0x30 > len(data):
                    raise ValueError(f"{row['source']}:{row['tree']}: bad descriptor")
                descriptors += 1
                formats[(data[item + 0x0E], data[item + 0x0F])] += 1
                for slot in range(4):
                    material = be32(data, item + 0x10 + slot * 4)
                    if material == 0:
                        continue
                    if material + 0x20 > len(data):
                        raise ValueError(f"{row['source']}:{row['tree']}: bad material")
                    material_slots += 1
                    if material in seen_materials:
                        continue
                    seen_materials.add(material)
                    keys[be32(data, material)] += 1
                    texture_count = be16(data, material + 0x0A)
                    for texture in range(texture_count):
                        ref = material + 0x20 + texture * 0x18
                        if ref + 0x18 > len(data):
                            raise ValueError(f"{row['source']}:{row['tree']}: bad texture")
                        gids.add(be32(data, ref))
    return objects, descriptors, material_slots, keys, formats, flags, gids


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--analysis-root", type=Path,
                        default=Path("ac6_demo_work/ac6-static-analysis"))
    args = parser.parse_args()

    objects, descriptors, slots, keys, formats, flags, gids = ndxr_census(
        args.analysis_root)
    pairs, nsxr_descriptors = nsxr_pairs(args.analysis_root)
    print(f"mapparts_objects={objects} descriptors={descriptors} material_slots={slots}")
    print("material_keys=" + ",".join(f"0x{k:08X}:{v}" for k, v in keys.items()))
    print("vertex_formats=" + ",".join(
        f"{hi:02X}/{lo:02X}:{count}" for (hi, lo), count in formats.items()))
    print("flags=" + ",".join(f"{key}:{count}" for key, count in flags.items()))
    print(f"distinct_gidx={len(gids)} nsxr_descriptors={nsxr_descriptors}")
    for key in sorted(set(keys) | {key ^ 0x00040000 for key in keys}):
        entries = pairs.get(key, ())
        rendered = []
        for filename, offset, records in entries:
            shaders = ",".join(f"{stage}:{name}" for stage, _, name in records)
            rendered.append(f"{filename}@0x{offset:X}[{shaders}]")
        print(f"key=0x{key:08X} " + (" | ".join(rendered) or "ABSENT"))


if __name__ == "__main__":
    main()
