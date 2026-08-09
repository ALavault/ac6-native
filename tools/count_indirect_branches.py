#!/usr/bin/env python3
"""Count the indirect branches in a function, before you conclude you have read
its dispatch.

Cycle 1277 lost two cycles to this. `0x82263A50`, the camera manager's per-frame
update, has TWO `bctr` switches. One was read, its jump table decoded, and its
arms attributed -- correctly. The other, sixty instructions later, is the one
that handles the modes the game actually runs in, and nothing in the listing
said so, because a `bctr` looks identical whether or not another follows it.

That is "the instrument sampled a third of it" with a truncated CONTROL-FLOW
GRAPH rather than a truncated listing, and it is worse, because a truncated
listing at least ends abruptly. A dispatch you have fully decoded looks finished.

WHAT IT REPORTS, for a range of the flat image:

  bctr    dispatch, no link      -- a jump table or a computed goto
  bctrl   dispatch with link     -- a virtual call or a function pointer
  blr     return
  blrl    call through the link register (rare, and worth a second look)

and for each `bctr` it prints the preceding `lis`/`addi` pair when one is within
reach, which is how the jump table's address is built here:

    82263e7c  lis  r12,-0x7dda
    82263e80  addi r12,r12,0x3e94      -> table at 0x82263E94

So the answer to "have I seen this function's dispatch" becomes a count you can
compare against what you decoded, rather than an impression.

usage: count_indirect_branches.py IMAGE STARTHEX ENDHEX
       count_indirect_branches.py analysis-input/ACE6_X360.exe 82263A50 82264700
exit 0 always; this is a survey, not a gate.
"""

import struct
import sys

BASE = 0x82000000
LOOKBACK = 8  # instructions to search backwards for the table's lis/addi pair


def decode_table_base(data, offset):
    """Recover a `lis rD,hi` + `addi rD,rD,lo` pair shortly before offset."""
    for back in range(1, LOOKBACK + 1):
        second_offset = offset - 4 * back
        if second_offset < 4:
            break
        second = struct.unpack_from(">I", data, second_offset)[0]
        if (second >> 26) != 14:  # addi
            continue
        destination = (second >> 21) & 0x1F
        source = (second >> 16) & 0x1F
        if destination != source:
            continue
        low = second & 0xFFFF
        signed = low - 0x10000 if low & 0x8000 else low
        for further in range(1, LOOKBACK + 1):
            first_offset = second_offset - 4 * further
            if first_offset < 0:
                break
            first = struct.unpack_from(">I", data, first_offset)[0]
            if (first >> 26) != 15:  # addis / lis
                continue
            if ((first >> 21) & 0x1F) != destination:
                continue
            high = first & 0xFFFF
            return (BASE + first_offset, BASE + second_offset,
                    ((high << 16) + signed) & 0xFFFFFFFF)
    return None


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: count_indirect_branches.py IMAGE STARTHEX ENDHEX")
        return 1
    try:
        with open(sys.argv[1], "rb") as handle:
            data = handle.read()
    except OSError as exc:
        print("  UNREADABLE  %s  %s" % (sys.argv[1], exc))
        return 1

    start = int(sys.argv[2], 16)
    end = int(sys.argv[3], 16)
    counts = {"bctr": 0, "bctrl": 0, "blr": 0, "blrl": 0}

    for address in range(start, end, 4):
        offset = address - BASE
        if offset < 0 or offset + 4 > len(data):
            continue
        word = struct.unpack_from(">I", data, offset)[0]
        if (word >> 26) != 19:  # the branch-conditional-to-register group
            continue
        extended = (word >> 1) & 0x3FF
        link = word & 1
        if extended == 528:  # bcctr
            name = "bctrl" if link else "bctr"
        elif extended == 16:  # bclr
            name = "blrl" if link else "blr"
        else:
            continue
        counts[name] += 1
        if name.startswith("bct"):
            pair = decode_table_base(data, offset)
            if pair is None:
                print("  0x%08X  %-5s   (no lis/addi pair within %d instructions;"
                      " the target may be a vtable slot)" % (address, name, LOOKBACK))
            else:
                lis_at, addi_at, table = pair
                print("  0x%08X  %-5s   table 0x%08X   built at 0x%08X / 0x%08X"
                      % (address, name, table, lis_at, addi_at))

    print("indirect_branches range=0x%08X..0x%08X  bctr=%d bctrl=%d blr=%d blrl=%d"
          % (start, end, counts["bctr"], counts["bctrl"], counts["blr"],
             counts["blrl"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
