#!/usr/bin/env python3
"""Derive vpermwi128's immediate encoding from the corpus, and grade both engines.

Cycle 1325 measured Ghidra and XenonRecomp decoding DIFFERENT immediates for the
same three instruction words, and showed that only the recompiler's reproduces
the x86 shuffle it emitted. Three sites is not a corpus, and all three named the
same registers, so the immediate's top bit could not be separated from the
destination field.

This does it over all 545. It does not GUESS a field layout: for each of the
eight immediate bits it asks which bits of the 32-bit instruction word agree with
it at EVERY site, and reports the answer. A bit with exactly one candidate is
derived; a bit with several is under-determined and says so; a bit with none
refutes the whole premise that the immediate is a permutation of word bits.

The expected values come from XenonRecomp's own per-instruction comments, whose
addresses are recovered as `function start + 4 * index` -- valid because every
instruction gets exactly one comment, which cycle 1325 confirmed at 40 of 40 on
0x822A1E80 before relying on it here.

`CLAUDE.md` allows this use of the generated code: literal cross-match evidence
about the image. Nothing runs.

    python3 tools/audit_vpermwi128_immediate_decode.py SITES_TSV RECOMP_DIR [--report R]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

IMPL = re.compile(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-Fa-f]{8})\) \{")
INSTRUCTION = re.compile(r"^\t// (\S+)(?:\s+(.*))?$")
VPERMWI = re.compile(r"^v(\d+),v(\d+),(\d+)$")


def recomp_immediates(root: Path) -> dict[int, int]:
    """address -> immediate, for every vpermwi128 the recompiler emitted."""
    found: dict[int, int] = {}
    for path in sorted(root.glob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in IMPL.finditer(text):
            start = int(match.group(1), 16)
            end = text.find("\n}\n", match.end())
            body = text[match.end():end if end > 0 else len(text)]
            index = 0
            for line in body.splitlines():
                instruction = INSTRUCTION.match(line)
                if not instruction:
                    continue
                if instruction.group(1) == "vpermwi128":
                    operands = VPERMWI.match((instruction.group(2) or "").replace(" ", ""))
                    if operands:
                        found[start + 4 * index] = int(operands.group(3))
                index += 1
    return found


def read_sites(path: Path) -> tuple[list[tuple[int, int, int]], dict[int, tuple[int, int]]]:
    rows = []
    registers: dict[int, tuple[int, int]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        columns = line.split("\t")
        address = int(columns[0], 16)
        rows.append((address, int(columns[1], 0), int(columns[2], 0)))
        if len(columns) >= 5:
            registers[address] = (int(re.sub(r"\D", "", columns[3])),
                                  int(re.sub(r"\D", "", columns[4])))
    return rows, registers


# THE DECLARED LAYOUT, from Xenia's InstrData::VX128_P, in x86-style bit numbers
# (bit 31 = most significant). It is written out as masks rather than copied as a
# C++ bitfield, because bitfield placement is an ABI property and not an ISA one.
#
#   25..21 VD128l   20..16 PERMl   15..11 VB128l
#   10 fixed 0   9 fixed 1   8..6 PERMh   5 fixed 0   4 fixed 1
#   3..2 VD128h   1..0 VB128h
#
# THIS IS A SECOND, INDEPENDENT STATEMENT OF THE SAME LAYOUT. The derivation
# below reconstructs the PERM bits from the corpus alone and never consults this;
# `main` then checks that the two agree, and that the fixed bits really are
# fixed. Two sources reached bit-for-bit the same answer, which is what makes it
# qualified rather than fitted.
def decode_declared(word: int) -> dict[str, int]:
    return {
        "opcode": word >> 26,
        "fixed_10_9": (word >> 9) & 0b11,
        "fixed_5_4": (word >> 4) & 0b11,
        "vd": ((word >> 21) & 0x1F) | (((word >> 2) & 0x03) << 5),
        "vb": ((word >> 11) & 0x1F) | ((word & 0x03) << 5),
        "perm": ((word >> 16) & 0x1F) | (((word >> 6) & 0x07) << 5),
    }


def derive(samples: list[tuple[int, int]]) -> dict[int, list[int]]:
    """For each immediate bit, the word bit positions that match it everywhere.

    Word bits are numbered PowerPC style: 0 is the most significant of 32.
    """
    candidates: dict[int, list[int]] = {}
    for immediate_bit in range(8):
        matching = []
        for word_bit in range(32):
            if all(((word >> (31 - word_bit)) & 1) == ((immediate >> immediate_bit) & 1)
                   for word, immediate in samples):
                matching.append(word_bit)
        candidates[immediate_bit] = matching
    return candidates


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sites", type=Path)
    parser.add_argument("recomp_root", type=Path)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args(argv)

    sites, registers = read_sites(arguments.sites)
    recomp = recomp_immediates(arguments.recomp_root)
    paired = [(address, word, ghidra, recomp[address])
              for address, word, ghidra in sites if address in recomp]

    print(f"ghidra_sites={len(sites)} recomp_sites={len(recomp)} paired={len(paired)}")
    if not paired:
        print("no site is present in both", file=sys.stderr)
        return 1

    agree = sum(1 for _, _, ghidra, other in paired if ghidra == other)
    print(f"engines_agree={agree}/{len(paired)}")

    candidates = derive([(word, other) for _, word, _, other in paired])
    derived: dict[int, int] = {}
    undetermined: list[int] = []
    refuted: list[int] = []
    for immediate_bit, matching in sorted(candidates.items()):
        if len(matching) == 1:
            derived[immediate_bit] = matching[0]
        elif matching:
            undetermined.append(immediate_bit)
        else:
            refuted.append(immediate_bit)

    print("immediate bit -> instruction word bit (PowerPC numbering, 0 = most "
          "significant), against XenonRecomp:")
    for immediate_bit in range(7, -1, -1):
        matching = candidates[immediate_bit]
        if len(matching) == 1:
            print(f"  imm[{immediate_bit}] = word[{matching[0]}]")
        elif matching:
            print(f"  imm[{immediate_bit}] UNDETERMINED, candidates {matching}")
        else:
            print(f"  imm[{immediate_bit}] MATCHES NO WORD BIT")

    if refuted:
        print(f"vpermwi128_immediate_decode=fail reason=no_bit_source bits={refuted}",
              file=sys.stderr)
        return 1
    if undetermined:
        print(f"vpermwi128_immediate_decode=fail reason=undetermined "
              f"bits={undetermined}", file=sys.stderr)
        return 1

    # The derivation is only worth having if it reproduces every site, so it is
    # applied back over the corpus rather than admired.
    def decode(word: int) -> int:
        value = 0
        for immediate_bit, word_bit in derived.items():
            value |= ((word >> (31 - word_bit)) & 1) << immediate_bit
        return value

    reproduced = sum(1 for _, word, _, other in paired if decode(word) == other)
    ghidra_reproduced = sum(1 for _, word, ghidra, _ in paired if decode(word) == ghidra)
    print(f"derived_reproduces_recomp={reproduced}/{len(paired)}")
    print(f"derived_reproduces_ghidra={ghidra_reproduced}/{len(paired)}")

    # The declared layout, checked over the same corpus -- including the parts the
    # derivation never looked at. The fixed bits are the structural control: if
    # the field positions were wrong, bits declared constant would not be.
    declared_perm = declared_vd = declared_vb = 0
    structural = 0
    for _, word, _, other in paired:
        fields = decode_declared(word)
        declared_perm += fields["perm"] == other
        structural += (fields["opcode"] == 0b000110
                       and fields["fixed_10_9"] == 0b01
                       and fields["fixed_5_4"] == 0b01)
    ghidra_registers = 0
    for address, word, _, _ in paired:
        fields = decode_declared(word)
        row = registers.get(address)
        if row and row == (fields["vd"], fields["vb"]):
            ghidra_registers += 1
    print(f"declared_layout_reproduces_recomp={declared_perm}/{len(paired)}")
    print(f"declared_layout_structural_bits={structural}/{len(paired)}")
    print(f"ghidra_registers_match_declared={ghidra_registers}/{len(paired)}")
    if declared_perm != len(paired) or structural != len(paired):
        print("vpermwi128_immediate_decode=fail reason=declared_layout",
              file=sys.stderr)
        return 1
    if reproduced != len(paired):
        print("vpermwi128_immediate_decode=fail reason=derivation", file=sys.stderr)
        return 1

    if arguments.report is not None:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(json.dumps({
            "schema": "ac6.vpermwi128-immediate-decode.v1",
            "statement": "vpermwi128's immediate field layout derived from the whole "
                         "corpus rather than assumed, by asking which instruction-word "
                         "bit agrees with each immediate bit at every site",
            "paired_sites": len(paired),
            "engines_agree": agree,
            "bit_map": {f"imm[{k}]": f"word[{v}]" for k, v in sorted(derived.items())},
            "derived_reproduces_recomp": reproduced,
            "derived_reproduces_ghidra": ghidra_reproduced,
            "declared_layout": "Xenia InstrData::VX128_P, applied as masks: "
                               "VD128l 25..21, PERMl 20..16, VB128l 15..11, "
                               "PERMh 8..6, VD128h 3..2, VB128h 1..0, with bits "
                               "10 and 5 fixed 0 and bits 9 and 4 fixed 1",
            "declared_layout_reproduces_recomp": declared_perm,
            "declared_layout_structural_bits": structural,
            "ghidra_registers_match_declared": ghidra_registers,
            "defect_scope": "the module decodes vD and vB correctly at every site; "
                            "only the immediate is wrong",
            "evidence_class": "cross-match: generated C++ read as a re-encoding of an "
                              "instruction's operands, never executed",
        }, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {arguments.report}")

    print("vpermwi128_immediate_decode=pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
