#!/usr/bin/env python3
"""Decode a range of PowerPC instructions straight out of the flat image.

Every instruction this campaign has quoted came from a Ghidra listing, and that
has cost real findings twice:

  - `exports/` silently truncates VMX128-heavy functions. `0x822A23D8` is 460
    instructions and `exports/` recovers 6 (INSTRUMENT_DISCIPLINE, and the
    reason `tools/check_listing_against_pdata.py` exists).
  - the decompiler drops what it cannot decode. `exports/82102568.json` carries
    "Control flow encountered bad instruction data" and *thirty-three* removed
    blocks, and the C it prints for the rest is a paraphrase of an incomplete
    read. Believing an arithmetic detail out of that text is believing a
    reconstruction of instructions Ghidra never decoded.

This decodes the bytes. It covers the integer, load/store and floating subset
that a field read is made of -- which is what the shape-reading cycles actually
need -- and prints anything else as `.long`, by design: an honest unknown beats
a guess, and a `.long` in the output is a signal to look, not a gap to ignore.

It does NOT know VMX128. Those come out as `.long` too, and a run of them is
exactly the signature that made `exports/` unreliable in the first place.

usage: ppc_read.py IMAGE START [END] [--count N]
       ppc_read.py analysis-input/ACE6_X360.exe 82102568 821026a0
exit 0 always; this is a reader, not a gate.
"""

import sys

BASE = 0x82000000

# Primary-opcode D-form loads and stores: mnemonic, and whether rA==0 means
# literal zero rather than r0. Only the second matters for reading a field
# displacement, which is the whole point of the tool.
DFORM = {
    32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu",
    36: "stw", 37: "stwu", 38: "stb", 39: "stbu",
    40: "lhz", 41: "lhzu", 42: "lha", 43: "lhau",
    44: "sth", 45: "sthu", 46: "lmw", 47: "stmw",
    48: "lfs", 49: "lfsu", 50: "lfd", 51: "lfdu",
    52: "stfs", 53: "stfsu", 54: "stfd", 55: "stfdu",
}

DFORM_ARITH = {
    7: "mulli", 8: "subfic", 12: "addic", 13: "addic.",
    14: "addi", 15: "addis", 24: "ori", 25: "oris",
    26: "xori", 27: "xoris", 28: "andi.", 29: "andis.",
}

CMP_D = {10: "cmpli", 11: "cmpi"}

# X-form (opcode 31) extended opcodes, by the 10-bit field at bits 21..30.
XFORM = {
    0: "cmp", 32: "cmpl",
    23: "lwzx", 55: "lwzux", 87: "lbzx", 119: "lbzux",
    151: "stwx", 183: "stwux", 215: "stbx", 247: "stbux",
    279: "lhzx", 311: "lhzux", 343: "lhax", 375: "lhaux",
    407: "sthx", 439: "sthux",
    535: "lfsx", 567: "lfsux", 599: "lfdx", 631: "lfdux",
    663: "stfsx", 695: "stfsux", 727: "stfdx", 759: "stfdux", 983: "stfiwx",
    8: "subfc", 10: "addc", 11: "mulhwu", 40: "subf",
    75: "mulhw", 104: "neg", 136: "subfe", 138: "adde",
    202: "addze", 234: "addme", 235: "mullw", 266: "add",
    459: "divwu", 491: "divw",
    24: "slw", 28: "and", 60: "andc", 124: "nor",
    284: "eqv", 316: "xor", 412: "orc", 444: "or", 476: "nand",
    536: "srw", 792: "sraw", 824: "srawi",
    922: "extsh", 954: "extsb", 986: "extsw",
    339: "mfspr", 467: "mtspr", 19: "mfcr", 144: "mtcrf",
    20: "lwarx", 150: "stwcx.", 598: "sync", 854: "eieio",
    #  64-bit
    21: "ldx", 53: "ldux", 149: "stdx", 181: "stdux",
    341: "lwax", 373: "lwaux", 27: "sld", 539: "srd",
    794: "srad", 233: "mulld", 457: "divdu", 489: "divd",
    986 + 0: "extsw",
}

# Opcode 63 / 59 extended opcodes at bits 26..30 (A-form) and 21..30 (X-form).
AFORM = {18: "fdiv", 20: "fsub", 21: "fadd", 22: "fsqrt", 23: "fsel",
         25: "fmul", 26: "frsqrte", 28: "fmsub", 29: "fmadd",
         30: "fnmsub", 31: "fnmadd"}
FXFORM = {0: "fcmpu", 12: "frsp", 14: "fctiw", 15: "fctiwz", 32: "fcmpo",
          40: "fneg", 72: "fmr", 136: "fnabs", 264: "fabs",
          814: "fctid", 815: "fctidz", 846: "fcfid", 583: "mffs",
          711: "mtfsf"}


def _simm(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def _hexs(v):
    return ("-0x%X" % -v) if v < 0 else ("0x%X" % v)


def decode(word, addr):
    op = word >> 26
    rd = (word >> 21) & 31
    ra = (word >> 16) & 31
    rb = (word >> 11) & 31
    d = _simm(word)
    rc = word & 1

    if op == 62:                                # std / stdu, and their update forms
        ds = (word & 0xFFFC)
        if ds & 0x8000:
            ds -= 0x10000
        return "%-8s r%d,%s(r%d)" % ("stdu" if word & 1 else "std", rd, _hexs(ds), ra)
    if op == 58:                                # ld / ldu
        ds = (word & 0xFFFC)
        if ds & 0x8000:
            ds -= 0x10000
        return "%-8s r%d,%s(r%d)" % ("ldu" if word & 1 else "ld", rd, _hexs(ds), ra)
    if op in DFORM:
        return "%-8s r%d,%s(r%d)" % (DFORM[op], rd, _hexs(d), ra) if op < 48 \
            else "%-8s f%d,%s(r%d)" % (DFORM[op], rd, _hexs(d), ra)
    if op in DFORM_ARITH:
        m = DFORM_ARITH[op]
        if m in ("ori", "oris", "xori", "xoris", "andi.", "andis."):
            return "%-8s r%d,r%d,0x%X" % (m, ra, rd, word & 0xFFFF)
        if m == "addis" and ra == 0:
            return "%-8s r%d,%s" % ("lis", rd, _hexs(d))
        if m == "addi" and ra == 0:
            return "%-8s r%d,%s" % ("li", rd, _hexs(d))
        return "%-8s r%d,r%d,%s" % (m, rd, ra, _hexs(d))
    if op in CMP_D:
        return "%-8s cr%d,r%d,%s" % (CMP_D[op] + ("w" if not (rd & 1) else ""),
                                     rd >> 2, ra, _hexs(d) if op == 11 else "0x%X" % (word & 0xFFFF))
    if op == 31:
        xo = (word >> 1) & 0x3FF
        m = XFORM.get(xo)
        if m is None:
            return ".long    0x%08X   # op31 xo=%d" % (word, xo)
        if m == "srawi":
            return "%-8s r%d,r%d,%d%s" % (m, ra, rd, rb, "." if rc else "")
        if m in ("cmp", "cmpl"):
            return "%-8s cr%d,r%d,r%d" % (m + "w", rd >> 2, ra, rb)
        if m in ("or", "and", "xor", "nor", "andc", "orc", "nand", "eqv",
                 "slw", "srw", "sraw", "sld", "srd", "srad"):
            if m == "or" and rd == rb:
                return "%-8s r%d,r%d" % ("mr", ra, rd)
            return "%-8s r%d,r%d,r%d%s" % (m, ra, rd, rb, "." if rc else "")
        if m in ("extsh", "extsb", "extsw"):
            return "%-8s r%d,r%d%s" % (m, ra, rd, "." if rc else "")
        if m in ("mfspr", "mtspr"):
            spr = ((word >> 16) & 0x1F) | (((word >> 11) & 0x1F) << 5)
            name = {1: "xer", 8: "lr", 9: "ctr"}.get(spr, "spr%d" % spr)
            return "%-8s r%d,%s" % (m, rd, name) if m == "mfspr" \
                else "%-8s %s,r%d" % (m, name, rd)
        if m.startswith("lf") or m.startswith("stf"):
            return "%-8s f%d,r%d,r%d" % (m, rd, ra, rb)
        if m in ("neg", "addze", "addme"):
            return "%-8s r%d,r%d%s" % (m, rd, ra, "." if rc else "")
        return "%-8s r%d,r%d,r%d%s" % (m, rd, ra, rb, "." if rc else "")
    if op in (20, 21, 23):                      # rlwimi, rlwinm, rlwnm
        sh, mb, me = rb, (word >> 6) & 31, (word >> 1) & 31
        m = {20: "rlwimi", 21: "rlwinm", 23: "rlwnm"}[op]
        return "%-8s r%d,r%d,%d,%d,%d%s" % (m, ra, rd, sh, mb, me, "." if rc else "")
    if op == 30:                                # rldicl and friends
        sh = ((word >> 11) & 31) | (((word >> 1) & 1) << 5)
        mb = ((word >> 6) & 31) | (((word >> 5) & 1) << 5)
        sub = (word >> 2) & 7
        m = {0: "rldicl", 1: "rldicr", 2: "rldic", 3: "rldimi"}.get(sub, "rld?")
        return "%-8s r%d,r%d,%d,%d" % (m, ra, rd, sh, mb)
    if op == 16:                                # bc
        bd = (word & 0xFFFC)
        if bd & 0x8000:
            bd -= 0x10000
        tgt = (bd if word & 2 else addr + bd) & 0xFFFFFFFF
        return "%-8s %d,%d,0x%08X%s" % ("bc" + ("l" if word & 1 else ""),
                                        rd, ra, tgt, "")
    if op == 18:                                # b
        li = word & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        tgt = (li if word & 2 else addr + li) & 0xFFFFFFFF
        return "%-8s 0x%08X" % ("b" + ("l" if word & 1 else ""), tgt)
    if op == 19:
        xo = (word >> 1) & 0x3FF
        if xo == 16:
            return "bclr" + ("l" if word & 1 else "")
        if xo == 528:
            return "bcctr" + ("l" if word & 1 else "")
        return ".long    0x%08X   # op19 xo=%d" % (word, xo)
    if op in (59, 63):
        xo5 = (word >> 1) & 31
        if xo5 in AFORM and (word >> 1) & 0x3E0 != 0 or xo5 in AFORM:
            m = AFORM[xo5] + ("s" if op == 59 else "")
            frc = (word >> 6) & 31
            if m.startswith(("fmadd", "fmsub", "fnmadd", "fnmsub", "fsel")):
                return "%-8s f%d,f%d,f%d,f%d" % (m, rd, ra, frc, rb)
            if m.startswith("fmul"):
                return "%-8s f%d,f%d,f%d" % (m, rd, ra, frc)
            return "%-8s f%d,f%d,f%d" % (m, rd, ra, rb)
        xo10 = (word >> 1) & 0x3FF
        if xo10 in FXFORM:
            m = FXFORM[xo10]
            if m in ("fcmpu", "fcmpo"):
                return "%-8s cr%d,f%d,f%d" % (m, rd >> 2, ra, rb)
            return "%-8s f%d,f%d" % (m, rd, rb)
        return ".long    0x%08X   # op%d xo=%d" % (word, op, xo10)
    return ".long    0x%08X   # op=%d" % (word, op)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 0
    image, start = argv[1], int(argv[2], 16)
    end = None
    count = None
    rest = argv[3:]
    i = 0
    while i < len(rest):
        if rest[i] == "--count":
            count = int(rest[i + 1])
            i += 2
        else:
            end = int(rest[i], 16)
            i += 1
    if end is None:
        end = start + 4 * (count if count else 64)
    data = open(image, "rb").read()
    for addr in range(start, end, 4):
        off = addr - BASE
        if off < 0 or off + 4 > len(data):
            print("0x%08X  <outside the image>" % addr)
            continue
        word = int.from_bytes(data[off:off + 4], "big")
        print("0x%08X  %08X  %s" % (addr, word, decode(word, addr)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
