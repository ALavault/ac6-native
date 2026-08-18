#!/usr/bin/env python3
"""Name a byte offset inside the Xbox 360 D3DDevice, from the XDK's own headers.

The demo's graphics frontier is read as raw displacements into the object at
0x10041A00 -- the context VdSetGraphicsInterruptCallback is given, which in the
XDK's D3D is the D3DDevice. That struct is open in d3d9.h, so the offsets can be
named instead of guessed.

Two checks make the arithmetic trustworthy rather than plausible:

  - m_pRing must land at +48 and m_pRingGuarantee at +56, because sub_821C57D0
    loads exactly those two, compares them, appends four dwords and stores the
    advanced pointer back to +48 -- a D3D ring append. Those two offsets sit
    before every array, so they check the header's prefix and nothing else.
  - m_Constants is __declspec(align(128)), so its computed offset must be a
    multiple of 128. That one does exercise the arrays in front of it.

An offset past the end is reported as past the end. The shipped header declares
the public device; the real allocation may be larger, and this tool will not
invent a name for something it cannot see.

usage: compute_d3ddevice_layout.py [--xdk DIR] [OFFSET ...]
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DWORD = 4
FIELD = re.compile(r"^\s+[A-Za-z_]\w*\s+(\w+)\s*(?:\[(\d+)\])?\s*;")
PACKETS = ["DESTINATIONPACKET", "WINDOWPACKET", "VALUESPACKET", "PROGRAMPACKET",
           "CONTROLPACKET", "TESSELLATORPACKET", "MISCPACKET", "POINTPACKET"]


def packet_dwords(text: str, name: str) -> int:
    end = text.index("} GPU_%s;" % name)
    start = text.rindex("typedef struct", 0, end)
    total = 0
    for line in text[start:end].splitlines()[2:]:
        match = FIELD.match(line)
        if match:
            total += int(match.group(2) or 1)
    if total == 0:
        raise SystemExit("GPU_%s parsed as empty" % name)
    return total


def layout(xdk: Path) -> list[tuple[str, int, int]]:
    gpu = (xdk / "d3d9gpu.h").read_text(errors="replace")
    members: list[tuple[str, int, int]] = []
    offset = 0

    def add(name: str, size: int, align: int = 4) -> None:
        nonlocal offset
        offset = (offset + align - 1) // align * align
        members.append((name, offset, size))
        offset += size

    add("m_Pending", 5 * 8)                      # UINT64 m_Mask[5]
    add("m_Predicated_PendingMask2", 8)
    add("m_pRing", 4)
    add("m_pRingLimit", 4)
    add("m_pRingGuarantee", 4)
    add("m_ReferenceCount", 4)
    # D3DRS_MAX/4 and D3DSAMP_MAX/4 entries of four-byte function pointers.
    add("m_SetRenderStateCall", (404 // 4) * DWORD)
    add("m_SetSamplerStateCall", (80 // 4) * DWORD)
    add("m_GetRenderStateCall", (404 // 4) * DWORD)
    add("m_GetSamplerStateCall", (80 // 4) * DWORD)
    # GPUFETCH_CONSTANT is a union of GPUVERTEX_FETCH_CONSTANT[3], each two
    # dwords; Alu is D3DVECTOR4[512]; Flow is the boolean/integer register file
    # as the D3D view spells it: 128/32 + 128/32 + 16 + 16 dwords.
    constants = 32 * 3 * 2 * DWORD + 512 * 16 + (4 + 4 + 16 + 16) * DWORD
    add("m_Constants", constants, align=128)
    add("m_ClipPlanes", 6 * 4 * DWORD)
    for name in PACKETS:
        add("m_" + name.replace("PACKET", "Packet").title().replace("packet", "Packet"),
            packet_dwords(gpu, name) * DWORD)
    return members


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xdk", type=Path,
                        default=Path("sdk/xdk-xenon-6132.6/XDK/include/xbox"))
    parser.add_argument("offsets", nargs="*")
    args = parser.parse_args()
    members = layout(args.xdk)
    by_name = {name: (offset, size) for name, offset, size in members}
    failures = []
    if by_name["m_pRing"][0] != 48:
        failures.append("m_pRing at %d, expected 48" % by_name["m_pRing"][0])
    if by_name["m_pRingGuarantee"][0] != 56:
        failures.append("m_pRingGuarantee at %d, expected 56"
                        % by_name["m_pRingGuarantee"][0])
    if by_name["m_Constants"][0] % 128 != 0:
        failures.append("m_Constants at %d is not 128-aligned"
                        % by_name["m_Constants"][0])
    for name, offset, size in members:
        print("  +%-6d 0x%-5X %-28s %d bytes" % (offset, offset, name, size))
    total = members[-1][1] + members[-1][2]
    print("public D3DDevice size = %d bytes (0x%X)" % (total, total))
    if failures:
        print("d3ddevice_layout=fail")
        print("\n".join("error: " + line for line in failures))
        return 1
    print("d3ddevice_layout=pass checks=3")
    for text in args.offsets:
        want = int(text, 0)
        hit = [(n, o, s) for n, o, s in members if o <= want < o + s]
        if hit:
            n, o, s = hit[0]
            print("  0x%X (%d) -> %s + %d" % (want, want, n, want - o))
        else:
            print("  0x%X (%d) -> past the end of the public struct (%d)"
                  % (want, want, total))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
