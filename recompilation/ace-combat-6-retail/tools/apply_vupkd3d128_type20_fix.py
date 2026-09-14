#!/usr/bin/env python3
"""Replace __builtin_debugtrap() for unimplemented vupkd3d128 type 20.

vupkd3d128 vD,vB,20 unpacks four float16 values from the source
register's u16[0..3] into float32 in the destination register's
u32[0..3].

With PPC_CONFIG_NON_VOLATILE_AS_LOCAL, registers v14-v127 are local
variables (plain `v127`), not context members (`ctx.v127`).  v0-v13
are always on ctx.
"""

import re
import sys
from pathlib import Path

CTX_THRESHOLD = 14


def _vreg(name: str) -> str:
    num = int(name[1:])
    return f"ctx.{name}" if num < CTX_THRESHOLD else name


def _expand(dst: str, src: str, elem: int) -> str:
    d = _vreg(dst)
    s = _vreg(src)
    return (
        f"\ttemp.u32 = {s}.u16[{elem}];\n"
        f"\t{{ uint32_t r = ((temp.u32 & 0x8000) << 16)"
        f" | (((temp.u32 & 0x7C00) + 0x1C000) << 13)"
        f" | ((temp.u32 & 0x03FF) << 13);\n"
        f"\t  if ((temp.u32 & 0x7C00) == 0)"
        f" r = (temp.u32 & 0x8000) << 16;\n"
        f"\t  {d}.u32[{elem}] = r; }}"
    )


def expand_vupkd3d128_20(dst: str, src: str) -> str:
    lines = []
    for elem in (3, 2, 1, 0):
        lines.append(_expand(dst, src, elem))
    return "\n".join(lines)


PATTERN = re.compile(
    r"(\t// vupkd3d128 (v\d+),(v\d+),20\n)"
    r"\t__builtin_debugtrap\(\);"
)


def patch(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    count = 0

    def replacer(m: re.Match) -> str:
        nonlocal count
        comment = m.group(1).rstrip("\n")
        dst = m.group(2)
        src = m.group(3)
        count += 1
        return comment + "\n" + expand_vupkd3d128_20(dst, src)

    result = PATTERN.sub(replacer, text)
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
            print(f"skip: {arg} (not a file)", file=sys.stderr)
            continue
        n = patch(p)
        if n > 0:
            print(f"{p.name}: {n} vupkd3d128 type-20 sites patched")
        total += n
    print(f"total: {total} sites patched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
