#!/usr/bin/env python3
"""Replace __builtin_debugtrap() for unimplemented vcmpbfp. (record form).

vcmpbfp. vD,vA,vB sets bit31 if vA>vB, bit30 if vA<-vB per element,
then updates CR6.  cr6 is a local variable in XenonRecomp functions.
v0-v13 are ctx members; v14+ are locals.
"""

import re
import sys
from pathlib import Path

CTX_THRESHOLD = 14


def _vreg(name: str) -> str:
    num = int(name[1:])
    return f"ctx.{name}" if num < CTX_THRESHOLD else name


def expand_vcmpbfp_dot(vd: str, va: str, vb: str) -> str:
    d = _vreg(vd)
    a = _vreg(va)
    b = _vreg(vb)
    return (
        f"\tctx.fpscr.enableFlushMode();\n"
        f"\t{{ PPCVRegister vcbfp_t{{}};\n"
        f"\tsimde_mm_store_ps(vcbfp_t.f32, simde_mm_and_ps("
        f"simde_mm_cmpgt_ps(simde_mm_load_ps({a}.f32), "
        f"simde_mm_load_ps({b}.f32)), "
        f"simde_mm_castsi128_ps(simde_mm_set1_epi32(int(0x80000000)))));\n"
        f"\tsimde_mm_store_ps({d}.f32, simde_mm_and_ps("
        f"simde_mm_cmplt_ps(simde_mm_load_ps({a}.f32), "
        f"simde_mm_xor_ps(simde_mm_load_ps({b}.f32), "
        f"simde_mm_castsi128_ps(simde_mm_set1_epi32(int(0x80000000))))), "
        f"simde_mm_castsi128_ps(simde_mm_set1_epi32(int(0x40000000)))));\n"
        f"\tsimde_mm_store_ps({d}.f32, simde_mm_or_ps("
        f"simde_mm_load_ps(vcbfp_t.f32), simde_mm_load_ps({d}.f32)));\n"
        f"\tcr6.setFromMask(simde_mm_castsi128_ps("
        f"simde_mm_or_si128(simde_mm_load_si128((simde__m128i*){d}.f32), "
        f"simde_mm_slli_epi32(simde_mm_load_si128((simde__m128i*){d}.f32), 1))), 0xF); }}"
    )


PATTERN = re.compile(
    r"(\t// vcmpbfp\. (v\d+),(v\d+),(v\d+)\n)"
    r"\t__builtin_debugtrap\(\);"
)


def patch(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    count = 0

    def replacer(m: re.Match) -> str:
        nonlocal count
        comment = m.group(1).rstrip("\n")
        vd, va, vb = m.group(2), m.group(3), m.group(4)
        count += 1
        return comment + "\n" + expand_vcmpbfp_dot(vd, va, vb)

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
            print(f"{p.name}: {n} vcmpbfp. sites patched")
        total += n
    print(f"total: {total} sites patched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
