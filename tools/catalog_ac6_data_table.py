#!/usr/bin/env python3
"""Emit or verify the qualified PAL DATA.TBL range catalogue."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import struct
import sys
from pathlib import Path


EXPECTED_SHA256 = "82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5"
EXPECTED_SIZE = 14824
EXPECTED_ENTRIES = 926
EXPECTED_PACKS = 2
HEADER = (
    "index", "group", "archive", "codec", "source_offset", "stored_size",
    "expanded_size", "source_end",
)


def catalogue_bytes(table: bytes) -> bytes:
    if len(table) != EXPECTED_SIZE or hashlib.sha256(table).hexdigest() != EXPECTED_SHA256:
        raise ValueError("unqualified DATA.TBL identity")
    count, packs = struct.unpack_from(">II", table)
    if count != EXPECTED_ENTRIES or packs != EXPECTED_PACKS:
        raise ValueError("unqualified DATA.TBL header")
    output = io.StringIO(newline="")
    writer = csv.writer(output, delimiter="\t", lineterminator="\n")
    writer.writerow(HEADER)
    ranges: set[tuple[str, int, int]] = set()
    for index in range(count):
        group, offset, stored, expanded = struct.unpack_from(
            ">IIII", table, 8 + index * 16
        )
        archive = "DATA01.PAC" if group & 0x01000000 else "DATA00.PAC"
        codec = "mode1-xor-raw" if group & 0x00020000 else "mode1-xor-deflate"
        identity = (archive, offset, stored)
        if not stored or not expanded or identity in ranges:
            raise ValueError(f"invalid or duplicate range at entry {index}")
        ranges.add(identity)
        writer.writerow((index, f"0x{group:08x}", archive, codec, offset,
                         stored, expanded, offset + stored))
    return output.getvalue().encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("table", type=Path)
    parser.add_argument("--catalogue", type=Path, required=True)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    try:
        expected = catalogue_bytes(args.table.read_bytes())
        if args.write:
            args.catalogue.parent.mkdir(parents=True, exist_ok=True)
            temporary = args.catalogue.with_name(args.catalogue.name + ".tmp")
            temporary.write_bytes(expected)
            temporary.replace(args.catalogue)
        elif not args.catalogue.is_file() or args.catalogue.read_bytes() != expected:
            raise ValueError("catalogue differs from qualified DATA.TBL")
    except (OSError, ValueError, struct.error) as error:
        print(f"data_table_catalogue=fail error={error}", file=sys.stderr)
        return 1
    print(f"data_table_catalogue=pass entries={EXPECTED_ENTRIES} packs={EXPECTED_PACKS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
