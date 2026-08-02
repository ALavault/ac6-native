#!/usr/bin/env python3
"""Map guest addresses to the function that contains them.

The store watchpoint records a writer as a guest lr -- a return address inside
the calling function, not its entry point. Turning that into a name needs the
function boundary table, which this project already has: ac6recomp_config.toml
carries 10 456 [functions] entries, one per recovered function start.

Resolution is "greatest start <= address", which is exact when the address falls
inside a listed function and reports the preceding function plus an offset when
it does not. The offset is printed either way, because a large offset is itself
information: it usually means the address lies in a region the recompiler never
recovered as a function, which is worth knowing before drawing conclusions from
the name.

Usage:
    tools/ac6-guest-sym.py 0x821386C4 0x821D7C00 ...
    tools/ac6-guest-sym.py --config ac6recomp_config.toml --stdin
"""

import argparse
import bisect
import re
import sys
from pathlib import Path

ENTRY = re.compile(r"^0x([0-9A-Fa-f]{8})\s*=", re.M)


def load_functions(config):
    text = Path(config).read_text(errors="replace")
    starts = sorted({int(m.group(1), 16) for m in ENTRY.finditer(text)})
    return starts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("addresses", nargs="*")
    parser.add_argument("--config", default="ac6recomp_config.toml")
    parser.add_argument("--stdin", action="store_true")
    args = parser.parse_args()

    starts = load_functions(args.config)
    if not starts:
        print(f"no [functions] entries in {args.config}", file=sys.stderr)
        return 2

    values = list(args.addresses)
    if args.stdin:
        values += [line.strip() for line in sys.stdin if line.strip()]

    for raw in values:
        try:
            address = int(raw, 16)
        except ValueError:
            continue
        index = bisect.bisect_right(starts, address) - 1
        if index < 0:
            print(f"0x{address:08X}  (below the first recovered function)")
            continue
        base = starts[index]
        print(f"0x{address:08X}  sub_{base:08X}+0x{address - base:X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
