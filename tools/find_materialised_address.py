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

usage: find_materialised_address.py IMAGE ADDRESS [ADDRESS...] [--lookahead N]
       find_materialised_address.py analysis-input/ACE6_X360.exe 82297540
exit 0 always; this is a search, not a gate.
"""

import struct
import sys

BASE = 0x82000000
LOOKAHEAD = 12  # instructions between the lis and its completion

# THE COUNT DEPENDS ON THIS WINDOW, and the tool used to print it without
# saying so. Measured on two targets:
#
#   lookahead    0x0002D3B4    0x82297540
#           4          138             6
#           8          142             6
#          12          144             6
#          32          151             6
#          64          157             8
#
# A wider window pairs a `lis` with an `ori` further away, which may be the
# same materialisation split by scheduling or may be two unrelated uses of one
# register. Neither 138 nor 144 is "correct": they are answers to different
# questions, and a delegated agent and I reported both without either being
# wrong. So the window is now printed with every result, and settable.


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
            # THE OPERAND FIELDS DIFFER BETWEEN THE TWO FORMS, and getting
            # this wrong cost a 2.6x under-count that a delegated agent caught
            # in cycle 1285.
            #   addi rD,rA,SIMM : rD = bits 6..10, rA (the SOURCE) = bits 11..15
            #   ori  rA,rS,UIMM : rA (the DEST) = bits 11..15, rS = bits 6..10
            # So the register that must match the `lis` is bits 11..15 for addi
            # and bits 6..10 for ori. Testing bits 11..15 for both works only
            # when the pair reuses one register -- `lis r10,2 ; ori r10,r10,x`
            # -- and silently drops every cross-register form such as
            # `lis r10,2 ; ori r21,r10,0xD3B4`.
            source = ((second >> 16) & 0x1F) if opcode == 14 else \
                     ((second >> 21) & 0x1F)
            if source != destination:
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
    global LOOKAHEAD
    arguments = sys.argv[1:]
    if "--lookahead" in arguments:
        index = arguments.index("--lookahead")
        LOOKAHEAD = int(arguments[index + 1])
        arguments = arguments[:index] + arguments[index + 2:]
    sys.argv = [sys.argv[0]] + arguments
    if len(sys.argv) < 3:
        print("usage: find_materialised_address.py IMAGE ADDRESS [ADDRESS...] [--lookahead N]")
        return 1
    try:
        with open(sys.argv[1], "rb") as handle:
            data = handle.read()
    except OSError as exc:
        print("  UNREADABLE  %s  %s" % (sys.argv[1], exc))
        return 1

    print("image %s, %d bytes, base 0x%08X, lookahead %d instructions"
          % (sys.argv[1], len(data), BASE, LOOKAHEAD))
    for argument in sys.argv[2:]:
        target = int(argument, 16)
        sites = materialisations(data, target)
        print("  0x%08X materialised at %d site(s) WITHIN %d instructions"
              % (target, len(sites), LOOKAHEAD))
        for site, form in sites:
            print("      0x%08X  lis + %s   <- a CANDIDATE; read the site" %
                  (site, form))
    return 0


if __name__ == "__main__":
    sys.exit(main())
