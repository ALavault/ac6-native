#!/usr/bin/env python3
"""Replace __builtin_debugtrap() for unresolved blrl (indirect call via LR).

The single observed pattern is `mtlr r3; blrl` in sub_8238F230, where
sub_823907E0 stores r3/r4/r5 to memory then returns with r3 intact as
a function pointer.  The fix replaces the trap with
PPC_CALL_INDIRECT_FUNC(ctx.r3.u32).
"""

import re
import sys
from pathlib import Path


PATTERN = re.compile(
    r"(// blrl )\n__builtin_debugtrap\(\);"
)

REPLACEMENT = r"// blrl\n\tPPC_CALL_INDIRECT_FUNC(ctx.r3.u32);"


def patch(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    result, count = PATTERN.subn(REPLACEMENT, text)
    if count > 0:
        path.write_text(result, encoding="utf-8")
    return count


def main() -> int:
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} FILE [FILE ...]", file=sys.stderr)
        return 1
    total = 0
    for arg in sys.argv[1:]:
        p = Path(arg)
        if not p.is_file():
            continue
        n = patch(p)
        if n > 0:
            print(f"{p.name}: {n} blrl sites patched")
        total += n
    print(f"total: {total} sites patched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
