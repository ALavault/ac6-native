#!/usr/bin/env python3
"""Propose confirmed-chunk entries for the interior entries the port cannot emit.

The oracle translates a guest function the first time it executes it, so its
dump directory is a set of addresses that ran. 689 of them are not function
starts in the canonical Ghidra atlas, which means codegen emits none of them
and each surfaces as an unqualified indirect call the moment the guest arrives.
0x820D3710, reached by pressing START during the title screen, is one.

The oracle only says WHICH addresses to look at. AGENTS.md forbids letting it
define boundaries, so everything written into an entry here comes from the
binary and from the Ghidra-derived atlas:

  owner   the atlas function whose range contains the address
  size    from the address to its first blr/bctr inclusive, bounded BOTH by
          the owner's own end and by the next declared function start, so an
          entry never spills past the function Ghidra drew and never swallows
          another declared function
  hash    sha256 of exactly those bytes in the extracted basefile

An address with no atlas owner is not proposed at all -- 125 of the 689 are in
that state and need a Ghidra pass, not a generated guess. Nor is one that lies
on, or whose extent spans, a switch-table label: XenonRecomp refuses a
function boundary there ("switch boundary at 0x820CBD00"), and it is right to,
because a jump-table arm is not a callable entry. Nor is one whose address a
local branch crosses: declaring it cuts the owner in two, and a branch from
one half to the other becomes a goto to a label that no longer exists in the
translation unit ("use of undeclared label 'loc_823273F0'").

A proposal is NOT ready to apply. Applying all 409 at once was tried and
XenonRecomp refused it in strict mode:

    unqualified callable target at 0x821AC60C

Declaring an interior entry splits its owner, and the tail of the split can
then call an address that used to be internal to one function and is now a
call between two. The boundary set therefore has to be closed under "every
target a declared function calls is itself declared", and this tool does not
compute that closure yet. Until it does, an emitted fragment is a candidate
list, not a change.

Adding these to config/confirmed-chunks.toml also renumbers every generated
line, which is a campaign-level decision; see
reports/AC6_DEMO_CODEGEN_BOUNDARY_COUPLING.md.

usage: propose_interior_entries.py ORACLE_LIST [--emit OUT.toml] [--limit N]
"""
from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ATLAS = ROOT / "analysis/demo/ac6-demo-static-decomp-atlas-v1.json"
BASEFILE = ROOT / ".build/Default.xex.base.bin"
BASE = 0x82000000
TERMINATORS = (0x4E800020, 0x4E800420)  # blr, bctr
MAX_EXTENT = 0x200


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("--atlas", type=Path, default=ATLAS)
    parser.add_argument("--basefile", type=Path, default=BASEFILE)
    parser.add_argument("--switch-tables", type=Path,
                        default=Path("recompilation/ace-combat-6-demo/"
                                     "build-codegen-on/codegen/"
                                     "demo-switch-tables.toml"))
    parser.add_argument("--emit", type=Path)
    parser.add_argument("--limit", type=int)
    arguments = parser.parse_args()

    image = arguments.basefile.read_bytes()
    word = lambda address: struct.unpack_from(">I", image, address - BASE)[0]
    atlas = json.loads(arguments.atlas.read_text())
    starts = {int(f["entry"], 16) for f in atlas["functions"]}
    ranges = sorted((int(lo, 16), int(hi, 16), int(f["entry"], 16))
                    for f in atlas["functions"] for lo, hi in f["ranges"])
    lows = [entry[0] for entry in ranges]

    text = arguments.oracle.read_text(errors="replace")
    executed = {int(m, 16) for m in
                re.findall(r"/hirdump_title_\d+/([0-9A-Fa-f]{8})\b", text)}
    interior = sorted(a for a in executed if a not in starts)

    switch_labels: set[int] = set()
    if arguments.switch_tables.is_file():
        import tomllib
        tables = tomllib.load(arguments.switch_tables.open("rb"))
        for table in tables.get("switch", []):
            for value in table.values():
                if isinstance(value, list):
                    switch_labels.update(v for v in value if isinstance(v, int))
                elif isinstance(value, int):
                    switch_labels.add(value)

    def crosses(entry_address: int, low: int, high: int) -> bool:
        """Does a branch inside [low, high] jump across entry_address?"""
        cursor = low
        while cursor <= high:
            instruction = word(cursor)
            opcode = instruction >> 26
            if opcode == 18:          # b / bl / ba / bla
                if instruction & 2:   # absolute, not a local branch
                    cursor += 4
                    continue
                offset = instruction & 0x03FFFFFC
                if offset & 0x02000000:
                    offset -= 0x04000000
                target = cursor + offset
            elif opcode == 16:        # bc and friends
                if instruction & 2:
                    cursor += 4
                    continue
                offset = instruction & 0xFFFC
                if offset & 0x8000:
                    offset -= 0x10000
                target = cursor + offset
            else:
                cursor += 4
                continue
            if low <= target <= high:
                lo, hi = (cursor, target) if cursor < target else (target, cursor)
                if lo < entry_address <= hi:
                    return True
            cursor += 4
        return False

    proposed, orphan, uncapped, switched, cut = [], 0, 0, 0, 0
    for address in interior:
        index = bisect.bisect_right(lows, address) - 1
        owner = None
        while index >= 0:
            low, high, entry = ranges[index]
            if low <= address <= high:
                owner = (low, high, entry)
                break
            index -= 1
        if owner is None:
            orphan += 1
            continue
        low, high, entry = owner
        # build_demo.py drops any manifest function a configured chunk
        # covers, so an extent reaching past the next declared start deletes
        # that function and leaves its callers with an unqualified target --
        # which is how XenonRecomp refused the first 409-entry attempt, at
        # 0x821AC60C swallowed by a 0x58-byte extent from 0x821AC5F8.
        following = [s for s in starts if address < s <= high]
        limit = min(following) if following else high + 1
        cursor = address
        terminated = False
        while cursor < limit and cursor - address < MAX_EXTENT:
            if word(cursor) in TERMINATORS:
                terminated = True
                break
            cursor += 4
        if not terminated:
            # No blr/bctr within the owner or within MAX_EXTENT. Emitting a
            # size here would be the search limit, not the function's extent,
            # and a wrong size is worse than no entry: the build would hash
            # and compile bytes that are not the callee.
            uncapped += 1
            continue
        size = min(cursor + 4, limit) - address
        if any(address <= label < address + size for label in switch_labels):
            switched += 1
            continue
        if crosses(address, low, high):
            cut += 1
            continue
        raw = image[address - BASE:address - BASE + size]
        proposed.append((address, size, entry, low, high,
                         hashlib.sha256(raw).hexdigest()))

    print(f"interior={len(interior)} proposed={len(proposed)} "
          f"orphan={orphan} unterminated={uncapped} switch={switched} "
          f"branch_cut={cut}")
    if arguments.emit is None:
        for address, size, entry, _, _, digest in proposed[:arguments.limit or 5]:
            print(f"  0x{address:08X} size=0x{size:X} owner=0x{entry:08X} "
                  f"sha256={digest[:16]}")
        return 0

    lines = ["# Generated by tools/propose_interior_entries.py. NOT APPLIED.",
             "# Each address was executed by the oracle and is interior to a",
             "# canonical Ghidra function; the extent and hash come from the",
             "# basefile, never from the oracle.", ""]
    for address, size, entry, low, high, digest in proposed[:arguments.limit]:
        lines += [
            "[[function]]",
            f"address = 0x{address:08X}",
            f"size = 0x{size:X}",
            'kind = "callable-inner-chunk"',
            f'evidence = "canonical-ghidra:ace-combat-6-demo/Default.xex '
            f'owner=0x{entry:08X} range 0x{low:08X}..0x{high:08X}; extent to '
            f'first blr/bctr; oracle-executed candidate"',
            f'byte_sha256 = "{digest}"',
            "",
        ]
    arguments.emit.write_text("\n".join(lines), encoding="utf-8")
    print(f"emitted={min(len(proposed), arguments.limit or len(proposed))} "
          f"path={arguments.emit}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
