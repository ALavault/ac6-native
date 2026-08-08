#!/usr/bin/env python3
"""Read an MDLP bundle index and report what it holds.

The MDLP header, validated on Mission 01's `idx_0009/001_MDLP.mdlp` (cycle 1157):

    +0x00  'MDLP'
    +0x04  entry count
    +0x08  total size - equals the file size exactly
    +0x0C  offset of the entry table
    +0x10  base the table's offsets are relative to

Every entry is an `FHM ` bundle. Three checks the structure has to pass, and
does: all entries carry the signature, the offsets are monotonic, and the
declared total size matches the file. A wrong table offset or stride breaks all
three at once, which is why they are worth running rather than assuming.

Retail bytes are inputs and are never committed; this prints, it does not copy.

usage: ac6_mdlp_index.py MDLP [--census]
"""

import argparse
import collections
import re
import struct
import sys

CHUNKS = (b"NDXR", b"NTXR", b"MATE", b"GIDX", b"NSXR")


def read_u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mdlp")
    parser.add_argument("--census", action="store_true",
                        help="scan every entry for chunk signatures")
    args = parser.parse_args()

    with open(args.mdlp, "rb") as handle:
        data = handle.read()

    if len(data) < 0x14 or data[:4] != b"MDLP":
        sys.exit("not an MDLP")

    count = read_u32(data, 0x04)
    declared = read_u32(data, 0x08)
    table = read_u32(data, 0x0C)
    base = read_u32(data, 0x10)
    print(f"entries {count}  table {table:#x}  base {base:#x}")
    print(f"declared size {declared} vs file {len(data)}: "
          f"{'match' if declared == len(data) else 'MISMATCH'}")

    if table + 4 * count > len(data):
        sys.exit("entry table runs past the file")
    offsets = [read_u32(data, table + 4 * i) for i in range(count)]

    signed = sum(1 for o in offsets
                 if base + o + 4 <= len(data) and data[base + o:base + o + 4] == b"FHM ")
    print(f"entries starting with 'FHM ': {signed}/{count}")
    print(f"offsets monotonic: {offsets == sorted(offsets)}")

    if not args.census:
        return
    bounds = offsets + [declared - base]
    census = collections.Counter()
    per_entry = []
    for index in range(count):
        blob = data[base + bounds[index]:base + bounds[index + 1]]
        found = collections.Counter(
            match.group().decode() for match in re.finditer(b"|".join(CHUNKS), blob))
        census.update(found)
        per_entry.append((index, len(blob), dict(found)))
    print("\ntotal chunk census:", dict(census))
    print("\nfirst eight entries:")
    for index, size, found in per_entry[:8]:
        print(f"  entry {index:2d}  {size:>9} bytes  {found}")


if __name__ == "__main__":
    main()
