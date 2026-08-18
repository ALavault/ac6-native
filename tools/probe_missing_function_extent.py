#!/usr/bin/env python3
"""Propose an extent for a function that has no .pdata row, or refuse.

Eleven D3D functions the oracle executes are absent from the port because
.pdata does not declare them, so XenonRecomp never emitted them (see
reports/AC6_DEMO_ELEVEN_D3D_FUNCTIONS_ABSENT_FROM_THE_PORT.md). Declaring them
needs an extent, and .pdata is exactly what would have supplied it.

This walks instructions from the address to the first terminator and then
applies the four rules that boundary expansion has already failed on, each of
which cost a reverted attempt:

  1. it must not swallow a declared start -- any confirmed-chunks address or
     .pdata row strictly inside the extent is a refusal, not a merge;
  2. a bctr inside means a switch, whose labels this cannot see, so refuse;
  3. a local branch that targets past the terminator means the extent is short,
     so extend to cover it and re-check;
  4. no terminator within the scan limit is a refusal.

A refusal is the useful answer here. The tool prints the reason and proposes
nothing, because a boundary placed on a guess is worse than a missing function:
the port would run invented code instead of trapping.

usage: probe_missing_function_extent.py IMAGE ADDR [ADDR ...] [--limit N]
"""
from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

BASE = 0x82000000
BLR = 0x4E800020
BCTR = 0x4E800420


def pdata_starts(image: bytes) -> set[int]:
    starts = set()
    for offset in range(0x79E00, 0x89FB0, 8):
        if offset + 8 > len(image):
            break
        address, _flags = struct.unpack_from(">II", image, offset)
        if BASE <= address < BASE + len(image):
            starts.add(address)
    return starts


def declared_starts(path: Path) -> set[int]:
    if not path.is_file():
        return set()
    return {int(value, 16)
            for value in re.findall(r"address\s*=\s*0x([0-9A-Fa-f]+)",
                                    path.read_text())}


def probe(image: bytes, address: int, limit: int, forbidden: set[int]):
    word = lambda a: struct.unpack_from(">I", image, a - BASE)[0]
    end = None
    targets: set[int] = set()
    cursor = address
    while cursor < address + limit * 4:
        instruction = word(cursor)
        if instruction == BCTR:
            return None, "bctr at 0x%08X: a switch whose labels are not visible here" % cursor
        opcode = instruction >> 26
        if opcode == 18:                       # b / bl
            offset = instruction & 0x03FFFFFC
            if offset & 0x02000000:
                offset -= 0x04000000
            target = (0 if instruction & 2 else cursor) + offset
            if not instruction & 1:            # b, not bl
                targets.add(target)
        elif opcode == 16:                     # bc
            offset = instruction & 0xFFFC
            if offset & 0x8000:
                offset -= 0x10000
            targets.add((0 if instruction & 2 else cursor) + offset)
        if instruction == BLR:
            end = cursor + 4
            if not any(address < t < address + limit * 4 and t >= end
                       for t in targets):
                break
        cursor += 4
    if end is None:
        return None, "no blr within %d instructions" % limit
    while any(end <= t < address + limit * 4 for t in targets):
        end = max(t for t in targets if t >= end) + 4
    swallowed = sorted(a for a in forbidden if address < a < end)
    if swallowed:
        return None, "would swallow declared start 0x%08X" % swallowed[0]
    return end - address, None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("addresses", nargs="+")
    parser.add_argument("--limit", type=int, default=400)
    parser.add_argument("--chunks", type=Path,
                        default=Path("recompilation/ace-combat-6-demo/config/confirmed-chunks.toml"))
    args = parser.parse_args()
    image = args.image.read_bytes()
    forbidden = pdata_starts(image) | declared_starts(args.chunks)
    print("forbidden starts=%d" % len(forbidden))
    proposed = 0
    for text in args.addresses:
        address = int(text, 0)
        size, reason = probe(image, address, args.limit, forbidden)
        if size is None:
            print("  0x%08X  REFUSED: %s" % (address, reason))
        else:
            proposed += 1
            print("  0x%08X  size=%d (%d instructions)" % (address, size, size // 4))
    print("extent_probe proposed=%d refused=%d"
          % (proposed, len(args.addresses) - proposed))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
