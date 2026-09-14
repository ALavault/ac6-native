#!/usr/bin/env python3
"""Replace __builtin_debugtrap() for unimplemented vpkd3d128 v0,v1,3,1,3.

vpkd3d128 vD,vB,3,1,3 packs float32 elements from vB into float16
values in the high half-words of vD (u16[7] and u16[6]).  The
rexglue-generated code (cross-match evidence only) implements the
conversion with clamping, denorm handling and sign preservation.

Only the single observed operand triple (v0,v1,3,1,3) is patched.
"""

import re
import sys
from pathlib import Path


REPLACEMENT = """\tctx.fpscr.enableFlushMode();
\ttemp.u32 = (ctx.v1.u32[3]&0x7FFFFFFF);
\tvTemp.u8[0] = (temp.f32 != temp.f32) || (temp.f32 > 65504.0f) ? 0xFF : ((ctx.v1.u32[3]&0x7f800000)>>23);
\ttemp.u16 = vTemp.u8[0] != 0xFF ? ((ctx.v1.u32[3]&0x7FE000)>>13) : 0x0;
\tctx.v0.u16[7] = vTemp.u8[0] != 0xFF ? (vTemp.u8[0] > 0x70 ? (((vTemp.u8[0]-0x70)<<10)+temp.u16) : (0x71-vTemp.u8[0] > 31 ? 0x0 : ((0x400+temp.u16)>>(0x71-vTemp.u8[0])))) : 0x7FFF;
\tctx.v0.u16[7] |= ((ctx.v1.u32[3]&0x80000000)>>16);
\ttemp.u32 = (ctx.v1.u32[2]&0x7FFFFFFF);
\tvTemp.u8[0] = (temp.f32 != temp.f32) || (temp.f32 > 65504.0f) ? 0xFF : ((ctx.v1.u32[2]&0x7f800000)>>23);
\ttemp.u16 = vTemp.u8[0] != 0xFF ? ((ctx.v1.u32[2]&0x7FE000)>>13) : 0x0;
\tctx.v0.u16[6] = vTemp.u8[0] != 0xFF ? (vTemp.u8[0] > 0x70 ? (((vTemp.u8[0]-0x70)<<10)+temp.u16) : (0x71-vTemp.u8[0] > 31 ? 0x0 : ((0x400+temp.u16)>>(0x71-vTemp.u8[0])))) : 0x7FFF;
\tctx.v0.u16[6] |= ((ctx.v1.u32[2]&0x80000000)>>16);"""


PATTERN = re.compile(
    r"(\t// vpkd3d128 v0,v1,3,1,3\n)"
    r"\tctx\.fpscr\.enableFlushMode\(\);\n"
    r"\t__builtin_debugtrap\(\);"
)


def patch(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    count = 0

    def replacer(m: re.Match) -> str:
        nonlocal count
        count += 1
        return m.group(1).rstrip("\n") + "\n" + REPLACEMENT

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
            continue
        n = patch(p)
        if n > 0:
            print(f"{p.name}: {n} vpkd3d128 sites patched")
        total += n
    print(f"total: {total} sites patched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
