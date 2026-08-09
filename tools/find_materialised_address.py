#!/usr/bin/env python3
"""Find an address built in TWO instructions, which the other scans cannot see.

Every reference scan in this repository looks for one of two things: a branch
whose displacement resolves to the target, or a 32-bit word equal to it. A
PowerPC address materialised as `lis rD,hi` followed by `ori rD,rD,lo` or
`addi rD,rD,simm` is neither. It appears nowhere as a word and is the target of
no branch, so both scans return zero -- correctly, and uselessly.

INSTRUMENT_DISCIPLINE.md has warned about exactly this since the 0x29c80 case,
and the warning could only be followed by hand. It mostly was not, and it cost a
standing blocker:

  Cycle 1244 published "0x82297540 has zero instruction references" and listed
  the Set leader's FSM start as the single open hop of the placement chain. The
  address is materialised at SIX sites -- 0x82297A00, 0x82297AE0, 0x82297D00,
  0x8229835C, 0x822983A4, 0x82298480 -- each `lis`/`addi`, and at the first of
  them the result is stored to the stack and passed on. The negative was an
  artefact of the instruments available.

It scans the flat image rather than a Ghidra project, so it sees bytes analysis
never reached, and it reports the pairing form so a reader can check the site by
hand -- which they should, because a `lis` matching by value is a candidate and
not yet a reading.

Both high-half forms are tried: `ori` pairs with the plain high half, `addi`
pairs with the high half plus the carry the sign-extended low half implies.

usage: find_materialised_address.py IMAGE ADDRESS [ADDRESS...]
       find_materialised_address.py analysis-input/ACE6_X360.exe 82297540
exit 0 always; this is a search, not a gate.
"""

import struct
import sys

BASE = 0x82000000
LOOKAHEAD = 12  # instructions between the lis and its completion


def materialisations(data, target):
    """Return [(site, form)] for every lis+ori / lis+addi building target."""
    high = (target >> 16) & 0xFFFF
    low = target & 0xFFFF
    found = []
    for offset in range(0, len(data) - 7, 4):
        word = struct.unpack_from(">I", data, offset)[0]
        if (word >> 26) != 15:  # addis, which `lis` is
            continue
        immediate = word & 0xFFFF
        if immediate not in (high, (high + 1) & 0xFFFF):
            continue
        destination = (word >> 21) & 0x1F
        for step in range(1, LOOKAHEAD + 1):
            second_offset = offset + 4 * step
            if second_offset + 4 > len(data):
                break
            second = struct.unpack_from(">I", data, second_offset)[0]
            opcode = second >> 26
            if opcode not in (24, 14):  # ori, addi
                continue
            if ((second >> 16) & 0x1F) != destination:
                continue
            value = second & 0xFFFF
            if opcode == 24 and immediate == high and value == low:
                found.append((BASE + offset, "ori"))
                break
            if opcode == 14:
                signed = value - 0x10000 if value & 0x8000 else value
                if ((immediate << 16) + signed) & 0xFFFFFFFF == target:
                    found.append((BASE + offset, "addi"))
                    break
    return found


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: find_materialised_address.py IMAGE ADDRESS [ADDRESS...]")
        return 1
    try:
        with open(sys.argv[1], "rb") as handle:
            data = handle.read()
    except OSError as exc:
        print("  UNREADABLE  %s  %s" % (sys.argv[1], exc))
        return 1

    print("image %s, %d bytes, base 0x%08X" % (sys.argv[1], len(data), BASE))
    for argument in sys.argv[2:]:
        target = int(argument, 16)
        sites = materialisations(data, target)
        print("  0x%08X materialised at %d site(s)" % (target, len(sites)))
        for site, form in sites:
            print("      0x%08X  lis + %s   <- a CANDIDATE; read the site" %
                  (site, form))
    return 0


if __name__ == "__main__":
    sys.exit(main())
