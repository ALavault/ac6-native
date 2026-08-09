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


def read_sites(path: Path) -> list[tuple[int, int, int]]:
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        columns = line.split("\t")
        rows.append((int(columns[0], 16), int(columns[1], 0), int(columns[2], 0)))
    return rows


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

    sites = read_sites(arguments.sites)
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
            "evidence_class": "cross-match: generated C++ read as a re-encoding of an "
                              "instruction's operands, never executed",
        }, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {arguments.report}")

    print("vpermwi128_immediate_decode=pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
