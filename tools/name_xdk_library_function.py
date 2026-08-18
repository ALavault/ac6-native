#!/usr/bin/env python3
"""Name a guest function by matching its bytes against the XDK's static libraries.

The demo links the XDK's D3D statically, so many of the functions this campaign
calls sub_XXXXXXXX are library code that Microsoft shipped with names in the
archive's COFF symbol tables. Matching is by raw bytes: a window from the guest
image is searched in each library member, and the containing symbol is reported.

Relocated fields differ between the library and the linked image -- branch
targets, lis/addi halves of an address -- so a single window often fails while
its neighbours match. The tool slides several windows and reports how many hit,
which is the honest measure of confidence: one hit is a coincidence to check,
eight is an identification.

It never renames anything on its own and it prints the window count with every
answer, because a match on one window of a common prologue means nothing.

CALIBRATION, measured rather than assumed. At --window 16 the noise floor is
about 3 of 60: CModeTaskTitle::update at 0x8218A7A8, which is certainly Namco's
own code, scores 3/60 against d3d9.lib and 2/60 against two others, each time
under a different name. So at that window size anything up to roughly 6/60 is
indistinguishable from noise, and only D3D::SwapCallback at 24/60 stood clear of
it. Sixteen bytes is four instructions, which any prologue can supply.

A LOW SCORE IS NOT ABSENCE. D3DDevice_Swap -- a confirmed library function --
scores 1/16 at --window 48, so a zero there says the windows were unlucky, not
that the function is absent from the library. Do not conclude "this is game
code" from a miss; that mistake was published once and retracted.

usage: name_xdk_library_function.py IMAGE ADDR [ADDR ...] [--libs DIR]
       [--window BYTES] [--windows N]
"""
from __future__ import annotations

import argparse
import struct
from collections import Counter
from pathlib import Path

BASE = 0x82000000


def archive_members(data: bytes):
    offset = 8
    while offset + 60 <= len(data):
        header = data[offset:offset + 60]
        name = header[:16].decode("ascii", "replace").strip()
        try:
            size = int(header[48:58].decode().strip())
        except ValueError:
            return
        yield name, offset + 60, size
        offset += 60 + size + (size & 1)


def coff_symbol(member: bytes, local: int) -> str | None:
    if len(member) < 20:
        return None
    # COFF header: NumberOfSections at 2, PointerToSymbolTable at 8,
    # NumberOfSymbols at 12. Reading them as one <HII at offset 2 lands on
    # TimeDateStamp instead, which is why an earlier pass reported no matches
    # for bytes that plainly matched.
    sections = struct.unpack_from("<H", member, 2)[0]
    symbol_pointer, symbol_count = struct.unpack_from("<II", member, 8)
    optional = struct.unpack_from("<H", member, 16)[0]
    if symbol_pointer == 0 or symbol_pointer > len(member):
        return None
    strings = member[symbol_pointer + symbol_count * 18:]
    section_of = None
    for index in range(sections):
        base = 20 + optional + index * 40
        if base + 40 > len(member):
            return None
        raw_size, raw_pointer = struct.unpack_from("<II", member, base + 16)
        if raw_pointer <= local < raw_pointer + raw_size:
            section_of = (index + 1, local - raw_pointer)
            break
    if section_of is None:
        return None
    section_number, relative = section_of
    best = None
    for index in range(symbol_count):
        entry = symbol_pointer + index * 18
        if entry + 18 > len(member):
            break
        raw = member[entry:entry + 8]
        value, number, _kind, storage, aux = struct.unpack_from("<IhHBB", member, entry + 8)
        if number != section_number or storage not in (2, 3) or value > relative:
            continue
        if raw[:4] == b"\0\0\0\0":
            string_offset = struct.unpack_from("<I", raw, 4)[0]
            end = strings.find(b"\0", string_offset)
            name = strings[string_offset:end].decode("ascii", "replace")
        else:
            name = raw.rstrip(b"\0").decode("ascii", "replace")
        # A COMDAT function shares its value with the section symbol that
        # names the section itself. Rank the real symbol above it rather than
        # reporting ".text".
        rank = (value, 1 if storage == 2 else 0, 0 if name.startswith(".") else 1)
        if best is None or rank > best[0]:
            best = (rank, name)
    return best[1] if best else None


def name_address(image: bytes, libraries: list[tuple[str, bytes]], address: int,
                 window: int, count: int):
    votes: Counter[tuple[str, str]] = Counter()
    for step in range(count):
        start = address - BASE + step * 4
        probe = image[start:start + window]
        if len(probe) < window:
            break
        for library, data in libraries:
            found = data.find(probe)
            if found < 0:
                continue
            for _member, member_start, size in archive_members(data):
                if member_start <= found < member_start + size:
                    name = coff_symbol(data[member_start:member_start + size],
                                       found - member_start)
                    if name:
                        votes[(library, name)] += 1
                    break
    return votes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("addresses", nargs="+")
    parser.add_argument("--libs", type=Path,
                        default=Path("sdk/xdk-xenon-6132.6/XDK/lib/xbox"))
    parser.add_argument("--window", type=int, default=48)
    parser.add_argument("--windows", type=int, default=16)
    args = parser.parse_args()
    image = args.image.read_bytes()
    libraries = [(path.name, path.read_bytes())
                 for path in sorted(args.libs.glob("*.lib"))]
    print("libraries=%d" % len(libraries))
    for text in args.addresses:
        address = int(text, 0)
        votes = name_address(image, libraries, address, args.window, args.windows)
        if not votes:
            print("  0x%08X  no library match in %d windows" % (address, args.windows))
            continue
        for (library, name), hits in votes.most_common(3):
            print("  0x%08X  %-42s %s  (%d/%d windows)"
                  % (address, name, library, hits, args.windows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
