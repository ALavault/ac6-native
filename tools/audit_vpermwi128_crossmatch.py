#!/usr/bin/env python3
"""Adjudicate vpermwi128's lane order by cross-match, over the whole population.

Cycles 1297-1306 left this open. The SLEIGH module assigns the LOW bit-pair of
the immediate to destination element 0; Xenia's emitter assigns the HIGH pair.
Xenia's own comment contradicts its code, and the two immediates that would
discriminate -- 0x1B and 0xE4 -- occur in none of the image's 545 sites, so the
image cannot run them. Cycle 1306 concluded it needed an execution oracle.

It does not. XenonRecomp generated C++ for THIS XEX, and every vpermwi128 became
an `_mm_shuffle_epi32` whose immediate is a re-encoding of the PPC one. The
mapping between the two encodings is only consistent under one reading, so the
generated corpus adjudicates by arithmetic.

`CLAUDE.md` allows exactly this use: the generated C++ is "literal cross-match
evidence only, never a source for native behaviour". A lane order is an
instruction's semantics, not the game's behaviour, and nothing here runs.

The four candidate readings:

    high-first  dst.e[i] = src.e[(imm >> (2*(3-i))) & 3]     (Xenia's code)
    low-first   dst.e[i] = src.e[(imm >> (2*i)) & 3]         (the SLEIGH module)

each combined with the recompiler storing PPC element 0 at either the lowest or
the highest x86 lane. `_mm_shuffle_epi32(a, imm)` puts a[(imm >> 2j) & 3] in
x86 lane j, counting from the low lane.

Usage:
    python3 tools/audit_vpermwi128_crossmatch.py RECOMP_OUTPUT_DIR [--report R]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

# `// vpermwi128 v13,v8,171` then the emitted intrinsic on the following line.
SITE = re.compile(
    r"//\s*vpermwi128\s+v(\d+),\s*v(\d+),\s*(\d+)\s*\n[^\n]*?"
    r"simde_mm_shuffle_epi32\([^,]*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")


def predict(ppc_immediate: int, high_first: bool, reversed_storage: bool) -> int:
    """The x86 shuffle immediate this reading implies."""
    def selector(element: int) -> int:
        shift = 2 * (3 - element) if high_first else 2 * element
        return (ppc_immediate >> shift) & 3

    x86 = 0
    for lane in range(4):
        # x86 lane `lane` holds PPC element `3 - lane` when storage is reversed.
        element = 3 - lane if reversed_storage else lane
        source = selector(element)
        # and it names its source in the same coordinates.
        field = 3 - source if reversed_storage else source
        x86 |= (field & 3) << (2 * lane)
    return x86


def collect(root: Path) -> list[tuple[int, int]]:
    pairs: list[tuple[int, int]] = []
    for path in sorted(root.glob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in SITE.finditer(text):
            pairs.append((int(match.group(3)), int(match.group(4), 0)))
    return pairs


# Xenia's own conformance vectors, at
# src/xenia/cpu/ppc/testing/instr_vpermwi128.s -- four cases with the source
# [00010203, 04050607, 08090A0B, 0C0D0E0F].
#
# CYCLES 1309 AND 1314 SAID THIS FILE DID NOT EXIST. It does. The grep that
# "proved" otherwise searched src/xenia/cpu/testing/, one directory too shallow;
# the per-instruction tests live under ppc/testing/ and there are 167 of them.
# A negative from a search whose population was never checked -- the seventeenth
# shape, and it stood for five cycles.
#
# They are checked here in Python, not through the harness, because 0x1B and 0xE4
# occur at none of the image's 545 sites: the image cannot run them.
CONFORMANCE = {
    0x1B: (0, 1, 2, 3),   # identity
    0xE4: (3, 2, 1, 0),   # reverse
    0x00: (0, 0, 0, 0),   # broadcast of x
    0xFF: (3, 3, 3, 3),   # broadcast of w
}


def check_conformance() -> list[str]:
    """The reading this file concludes, against Xenia's four published vectors."""
    failures = []
    for immediate, expected in CONFORMANCE.items():
        got = tuple((immediate >> (2 * (3 - lane))) & 3 for lane in range(4))
        if got != expected:
            failures.append(f"0x{immediate:02X}: high-first gives {got}, "
                            f"Xenia's test says {expected}")
    return failures


READINGS = {
    "high-first, reversed storage": (True, True),
    "high-first, direct storage": (True, False),
    "low-first, reversed storage": (False, True),
    "low-first, direct storage": (False, False),
}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("root", type=Path)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args(argv)

    conformance = check_conformance()
    print(f"xenia_conformance_vectors={len(CONFORMANCE)} "
          f"disagreements={len(conformance)}")
    for line in conformance:
        print(f"  {line}", file=sys.stderr)
    if conformance:
        print("vpermwi128_crossmatch=fail reason=conformance", file=sys.stderr)
        return 1

    pairs = collect(arguments.root)
    if not pairs:
        print("no vpermwi128 sites found", file=sys.stderr)
        return 1

    scores = {name: sum(1 for ppc, x86 in pairs if predict(ppc, *flags) == x86)
              for name, flags in READINGS.items()}
    distinct = len({ppc for ppc, _ in pairs})
    total = len(pairs)

    print(f"sites={total} distinct_immediates={distinct}")
    for name, hits in sorted(scores.items(), key=lambda item: -item[1]):
        print(f"  {name:32s} {hits:4d}/{total}")

    winners = [name for name, hits in scores.items() if hits == total]
    if len(winners) != 1:
        print(f"vpermwi128_crossmatch=fail winners={winners}", file=sys.stderr)
        return 1

    # A reading that explains everything is only decisive if the others do not.
    runner_up = max(hits for name, hits in scores.items() if name != winners[0])
    print(f"decisive: {winners[0]} explains {total}/{total}; "
          f"best rival explains {runner_up}/{total}")

    if arguments.report is not None:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(json.dumps({
            "schema": "ac6.vpermwi128-crossmatch.v1",
            "statement": "over every vpermwi128 the recompiler emitted for this XEX, "
                         "only one reading of the immediate reproduces the x86 shuffle "
                         "it chose; the SLEIGH module uses the other",
            "sites": total,
            "distinct_ppc_immediates": distinct,
            "scores": scores,
            "reading": winners[0],
            "runner_up_hits": runner_up,
            "xenia_conformance_vectors": {f"0x{k:02X}": list(v)
                                          for k, v in CONFORMANCE.items()},
            "evidence_class": "cross-match: generated C++ read as a re-encoding of an "
                              "instruction's semantics, never executed and never a "
                              "source for native behaviour",
            "most_common_immediates": [
                {"ppc": f"0x{value:02X}", "sites": count}
                for value, count in Counter(ppc for ppc, _ in pairs).most_common(5)
            ],
        }, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {arguments.report}")

    print("vpermwi128_crossmatch=pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
