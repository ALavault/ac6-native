#!/usr/bin/env python3
"""Compare a function's instruction stream between Ghidra and XenonRecomp.

The first step of qualifying a function contract, and it comes BEFORE reading any
result as a matrix: if the two engines do not agree on which instructions the
function contains, nothing downstream means anything.

XenonRecomp emits each PPC instruction as a `// mnemonic operands` comment above
the C++ it generated for it, so its view of the instruction stream is recoverable
without running anything. This lines that up against Ghidra's listing and stops
at the first disagreement.

`CLAUDE.md` allows exactly this use of the generated code: literal cross-match
evidence about the image, never executed and never a source for native
behaviour. Nothing here runs; two disassemblies are compared as text.

    <analyzeHeadless ... -postScript DumpRange.java START END > listing.txt>
    python3 tools/audit_recomp_vs_ghidra_listing.py listing.txt RECOMP_DIR SYMBOL
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# `PPC_FUNC_IMPL(__imp__sub_822A1E80) {` ... to the closing brace at column 0.
IMPL = "PPC_FUNC_IMPL(__imp__{symbol}) {{"
# A recomp instruction comment: one tab, `// `, then the disassembly.
RECOMP_INSTRUCTION = re.compile(r"^\t// (\S+)\s*(.*)$")
# A DumpRange line: `822a1e80  mflr r12`.
GHIDRA_INSTRUCTION = re.compile(r"^([0-9a-fA-F]{8})\s+(\S+)\s*(.*)$")


# EVERY EQUIVALENCE THIS FILE APPLIES IS LISTED HERE AND COUNTED.
#
# The two engines print the same 40 instructions of 0x822A1E80 in four different
# notations, and a comparison that quietly accepted them would also quietly
# accept a real difference. So each rule is named, each is applied only in the
# direction stated, and the tally is printed -- a run that reports zero
# rewrites and a run that reports two hundred are different evidence.
#
# What is NOT equated: register numbers, immediate VALUES, branch targets,
# mnemonic families. A difference in any of those is what this file is for.
APPLIED: dict[str, int] = {}


def _count(rule: str) -> None:
    APPLIED[rule] = APPLIED.get(rule, 0) + 1


def _canonical_number(token: str) -> str:
    """0x80 and 128 are the same immediate; -0x7dfb and -32251 are too."""
    try:
        return str(int(token, 0))
    except ValueError:
        return token


def normalise(mnemonic: str, operands: str) -> str:
    operands = re.sub(r"\s*,\s*", ",", operands.strip())
    operands = re.sub(r"\s+", " ", operands)

    # 1. Immediate radix. Ghidra prints hex, XenonRecomp prints decimal.
    def number(match: re.Match) -> str:
        canonical = _canonical_number(match.group(0))
        if canonical != match.group(0):
            _count("immediate radix")
        return canonical

    operands = re.sub(r"-?0x[0-9a-fA-F]+|(?<![\w.])-?\d+(?![\w.])", number, operands)

    mnemonic = mnemonic.strip()
    text = f"{mnemonic} {operands}".strip()

    # 2. Extended mnemonics for the link register. `mfspr rX,LR` IS `mflr rX`:
    #    same encoding, two spellings.
    moved = re.fullmatch(r"mfspr (r\d+),LR", text)
    if moved:
        _count("mfspr/LR -> mflr")
        return f"mflr {moved.group(1)}"
    moved = re.fullmatch(r"mtspr LR,(r\d+)", text)
    if moved:
        _count("mtspr/LR -> mtlr")
        return f"mtlr {moved.group(1)}"
    # The same for the count register.
    moved = re.fullmatch(r"mtspr CTR,(r\d+)", text)
    if moved:
        _count("mtspr/CTR -> mtctr")
        return f"mtctr {moved.group(1)}"
    moved = re.fullmatch(r"mfspr (r\d+),CTR", text)
    if moved:
        _count("mfspr/CTR -> mfctr")
        return f"mfctr {moved.group(1)}"

    # 3. `or rA,rB,rB` IS `mr rA,rB` -- but only when the two sources are the
    #    SAME register. `or r3,r31,r30` is not a move and is not rewritten.
    moved = re.fullmatch(r"or (r\d+),(r\d+),(r\d+)", text)
    if moved and moved.group(2) == moved.group(3):
        _count("or rA,rB,rB -> mr")
        return f"mr {moved.group(1)},{moved.group(2)}"

    # 4b. `clrlwi rA,rS,n` IS `rlwinm rA,rS,0,n,31`, same encoding.
    moved = re.fullmatch(r"clrlwi (r\d+),(r\d+),(\d+)", text)
    if moved:
        _count("clrlwi -> rlwinm")
        return f"rlwinm {moved.group(1)},{moved.group(2)},0,{moved.group(3)},31"

    # 4. `subi rA,rB,N` IS `addi rA,rB,-N`. The immediate is negated, so the
    #    VALUE still has to match after the rewrite.
    moved = re.fullmatch(r"subi (r\d+),(r\d+),(-?\d+)", text)
    if moved:
        _count("subi -> addi with a negated immediate")
        return f"addi {moved.group(1)},{moved.group(2)},{-int(moved.group(3))}"

    # 5. `vrN` and `vN` are the same register file spelled two ways. Ghidra's
    #    listing writes `vrN` for the VMX128 forms and `vN` for the AltiVec ones;
    #    XenonRecomp writes `vN` for all 128.
    #
    #    CYCLE 1328 RESTRICTED THIS TO N < 32 AND THAT WAS WRONG. The reasoning
    #    was that vr32..vr127 have no AltiVec spelling, so rewriting them would
    #    invent one -- true of the ISA, false of the recompiler, which simply
    #    calls every register vN. Cycle 1329 met `vr127` against `v127` at
    #    0x822A23F0 and the restriction turned a naming difference into a
    #    reported divergence. The INDEX still has to match, which is the part
    #    that carries meaning.
    def register(match: re.Match) -> str:
        _count("vrN -> vN")
        return f"v{match.group(1)}"

    text = re.sub(r"\bvr(\d+)\b", register, text)

    # 5b. `vupkd3d128` OPERAND ORDER. Ghidra prints `vD,IMM,vB` and XenonRecomp
    #     prints `vD,vB,IMM`. Both name the same three things; only the print
    #     order differs, and the rewrite REQUIRES the immediate to be in the
    #     middle on one side and last on the other, so a genuine operand swap
    #     between two REGISTERS still fails.
    moved = re.fullmatch(r"vupkd3d128 (v\d+),(\d+),(v\d+)", text)
    if moved:
        _count("vupkd3d128 vD,IMM,vB -> vD,vB,IMM")
        return f"vupkd3d128 {moved.group(1)},{moved.group(3)},{moved.group(2)}"

    # 6. THE (rA|0) RULE, RENDERED. In an indexed form, rA = r0 means the literal
    #    zero and not the contents of r0, and XenonRecomp prints the rule already
    #    applied: `lvlx v13,0,r8` where Ghidra prints `lvlx v13,r0,r8`. Same
    #    instruction, and the difference is which layer applies the rule.
    #
    #    That is worth more than a spelling note. Cycle 1296 measured this SLEIGH
    #    module emitting INT_ADD(r0, rB) with NO (rA|0) rule, and cycle 1317
    #    pinned it as the `lvx128-ra-is-r0` defect. The recompiler rendering the
    #    zero is a third, independent statement that the rule exists -- from a
    #    tool that had to get it right to generate working code.
    #
    #    Restricted to the indexed shape `mnem X,0,rN`: a bare 0 in the middle
    #    operand with a register after it. Anywhere else it is left alone.
    indexed = re.fullmatch(r"(\S+) ([^,]+),0,(r\d+)", text)
    if indexed:
        _count("(rA|0) rendered as 0 -> r0")
        return f"{indexed.group(1)} {indexed.group(2)},r0,{indexed.group(3)}"
    return text


def read_recomp(root: Path, symbol: str) -> list[str] | None:
    marker = IMPL.format(symbol=symbol)
    for path in sorted(root.glob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        start = text.find(marker)
        if start < 0:
            continue
        end = text.find("\n}\n", start)
        body = text[start:end if end > 0 else len(text)]
        found = []
        for line in body.splitlines():
            match = RECOMP_INSTRUCTION.match(line)
            if match:
                found.append(normalise(match.group(1), match.group(2)))
        return found
    return None


def read_listing(path: Path) -> list[tuple[int, str]]:
    found = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = GHIDRA_INSTRUCTION.match(line.strip())
        if match:
            found.append((int(match.group(1), 16),
                          normalise(match.group(2), match.group(3))))
    return found


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("listing", type=Path)
    parser.add_argument("recomp_root", type=Path)
    parser.add_argument("symbol")
    arguments = parser.parse_args(argv)

    recomp = read_recomp(arguments.recomp_root, arguments.symbol)
    if recomp is None:
        print(f"no PPC_FUNC_IMPL for {arguments.symbol} under {arguments.recomp_root}",
              file=sys.stderr)
        return 1
    listing = read_listing(arguments.listing)

    print(f"ghidra={len(listing)} recomp={len(recomp)}")
    # KNOWN MODULE DEFECTS ARE CLASSIFIED, NOT EQUATED.
    #
    # Ghidra decodes vpermwi128's immediate wrongly at 536 of 545 sites (cycle
    # 1326, derived over the whole corpus). Every function carrying one will
    # diverge here, and silently rewriting it would hide a defect this campaign
    # has measured. So it is reported by address, counted, and the run may still
    # pass -- the same treatment the vector suite gives its pinned defects.
    known = re.compile(r"^vpermwi128 (v\d+),(v\d+),(\d+)$")

    def is_known_defect(left: str, right: str) -> bool:
        first, second = known.fullmatch(left), known.fullmatch(right)
        return bool(first and second
                    and first.group(1) == second.group(1)
                    and first.group(2) == second.group(2)
                    and first.group(3) != second.group(3))

    defects: list[str] = []
    agreed = 0
    for index, (address, ghidra_text) in enumerate(listing):
        if index >= len(recomp):
            print(f"recomp stream ends at {index}; ghidra continues with "
                  f"0x{address:08X} {ghidra_text}", file=sys.stderr)
            print("recomp_vs_ghidra=fail reason=length", file=sys.stderr)
            return 1
        if (ghidra_text.lower() != recomp[index].lower()
                and is_known_defect(ghidra_text, recomp[index])):
            defects.append(f"0x{address:08X}: ghidra {ghidra_text}, "
                           f"recomp {recomp[index]}")
            agreed += 1
            continue
        if ghidra_text.lower() != recomp[index].lower():
            print(f"FIRST DIVERGENCE at index {index}, 0x{address:08X}",
                  file=sys.stderr)
            print(f"  ghidra: {ghidra_text}", file=sys.stderr)
            print(f"  recomp: {recomp[index]}", file=sys.stderr)
            print(f"agreed_before_divergence={agreed}", file=sys.stderr)
            print("recomp_vs_ghidra=fail reason=instruction", file=sys.stderr)
            return 1
        agreed += 1
    if len(recomp) != len(listing):
        print(f"ghidra stream ends at {len(listing)}; recomp continues with "
              f"{recomp[len(listing)]}", file=sys.stderr)
        print("recomp_vs_ghidra=fail reason=length", file=sys.stderr)
        return 1
    print(f"recomp_vs_ghidra=pass instructions={agreed} "
          f"known_defects={len(defects)}")
    for rule, count in sorted(APPLIED.items()):
        print(f"  equated: {rule} x{count}")
    for defect in defects:
        print(f"  KNOWN DEFECT vpermwi128 immediate at {defect}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
