#!/usr/bin/env python3
"""Compare a function's declared length against what a listing recovered.

Every truncation this campaign has been bitten by looks the same from inside:
the listing ends on an ordinary instruction and nothing says whether the
function did. `.pdata` says. Each `RUNTIME_FUNCTION` here is eight bytes -- a
`BeginAddress` and a packed word whose bits 8..29 hold the function's length in
INSTRUCTIONS -- so the answer is one subtraction, and it has never been
automatic.

It has cost real work:

  - `Ac6XenonDisasm` caps at 300 instructions per block. Cycle 1254 read 300 of
    a 912-instruction function and drew a negative over the third it had.
  - `exports/` silently truncates VMX128-heavy functions: 0x822A23D8 is 460
    instructions and `exports/` recovers 6; 0x8229ADF8 is 97 and recovers 26.
    Cycle 1279 caught that only because it checked every function it cited.

WHAT IT REPORTS, per address:

  declared      the .pdata length in instructions, or "no .pdata row"
  recovered     lines counted in a listing file, when one is supplied
  verdict       equal, TRUNCATED, or unverifiable

`.pdata` here is INCOMPLETE -- 8,246 rows for a program with more functions than
that -- so "no .pdata row" is a real answer and not a failure. 0x8234CDC0, the
registry insert, has none. When a function has no row this tool cannot help, and
says so rather than guessing a length.

usage:
  check_listing_against_pdata.py IMAGE ADDR [ADDR...]
  check_listing_against_pdata.py IMAGE ADDR --listing FILE
      FILE being a disassembly whose lines start with an 8-hex-digit address;
      only lines inside the function's declared span are counted.
exit 0 unless a listing was supplied and came up short.
"""

import re
import struct
import sys

BASE = 0x82000000
PDATA_START = 0x82079E00
PDATA_ROWS = 8246
LINE = re.compile(r"^([0-9a-fA-F]{8})\b")


def pdata_index(data):
    """Map BeginAddress -> length in instructions."""
    table = {}
    offset = PDATA_START - BASE
    for row in range(PDATA_ROWS):
        begin, packed = struct.unpack_from(">II", data, offset + row * 8)
        table[begin] = (packed >> 8) & 0x3FFFFF
    return table


def recovered_from(path, start, declared):
    """Count listing lines that fall inside the declared span."""
    end = start + 4 * declared
    seen = set()
    try:
        with open(path, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                match = LINE.match(line)
                if not match:
                    continue
                address = int(match.group(1), 16)
                if start <= address < end:
                    seen.add(address)
    except OSError as exc:
        return None, str(exc)
    return len(seen), None


def main() -> int:
    arguments = sys.argv[1:]
    listing = None
    if "--listing" in arguments:
        index = arguments.index("--listing")
        listing = arguments[index + 1]
        arguments = arguments[:index] + arguments[index + 2:]
    if len(arguments) < 2:
        print("usage: check_listing_against_pdata.py IMAGE ADDR [ADDR...]"
              " [--listing FILE]")
        return 1

    try:
        with open(arguments[0], "rb") as handle:
            data = handle.read()
    except OSError as exc:
        print("  UNREADABLE  %s  %s" % (arguments[0], exc))
        return 1

    table = pdata_index(data)
    short = 0
    for argument in arguments[1:]:
        address = int(argument, 16)
        declared = table.get(address)
        if declared is None:
            print("  0x%08X  no .pdata row -- the table is incomplete here, so "
                  "this tool cannot verify the listing" % address)
            continue
        if listing is None:
            print("  0x%08X  declared %d instructions (0x%08X..0x%08X)"
                  % (address, declared, address, address + 4 * declared - 4))
            continue
        count, error = recovered_from(listing, address, declared)
        if count is None:
            print("  0x%08X  listing unreadable: %s" % (address, error))
            return 1
        verdict = "equal" if count == declared else "TRUNCATED"
        if count != declared:
            short += 1
        print("  0x%08X  declared %d  recovered %d  %s"
              % (address, declared, count, verdict))

    print("listing_against_pdata short=%d" % short)
    return 0 if short == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
