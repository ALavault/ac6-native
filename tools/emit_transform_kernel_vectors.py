#!/usr/bin/env python3
"""Reduce the transform-kernel capsules to a committed vector table.

The retail side is thirteen micro-executions of 0x822A1E80 and its three
rotations, and running them needs Ghidra. `ctest` must not. Each run becomes one
line -- what was called, how the basis was seeded, the angles, and the three
resulting rows as raw big-endian float bits -- and
tests/retail_transform_tests.cpp reads the table.

Raw bits, not decimals: the rotation arithmetic is compared exactly and only the
trigonometry carries a tolerance, so the table must not round away the thing
being compared.

    python3 tools/emit_transform_kernel_vectors.py --workdir W --output PATH
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from audit_transform_kernel_microexec import cases  # noqa: E402

HEADER = """\
# Retail vectors for the transform kernel, micro-executed from 0x822A1E80 and
# its three rotations. Every row is one run: what was called, how the basis was
# seeded, the angle(s), and the three resulting rows as raw big-endian float bits.
#
# Committed so tests/retail_transform_tests.cpp runs inside ctest without Ghidra.
# Regenerate with tools/emit_transform_kernel_vectors.py.
#
# The `sentinel` seed is rows (1,2,3,4), (5,6,7,8), (9,10,11,12) -- every word
# distinct, no two rows or columns alike -- with (17,29,43,61) in the 16 bytes
# 0x822A1E80 never writes. The `identity` seed is all zeros; 0x822A1E80 writes
# the identity basis itself out of .rodata.
#
# columns: case kind seed angles row0 row1 row2"""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--workdir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)

    rows = []
    for case in cases({}):
        document = json.loads(
            (arguments.workdir / "out" / f"{case['name']}.json").read_text())
        dumps = document.get("region_dumps", [])
        if not dumps:
            print(f"{case['name']}: no region dump", file=sys.stderr)
            return 1
        blob = bytes.fromhex(dumps[0]["after_hex"])
        basis = ["\t".join(
            f"0x{struct.unpack_from('>I', blob, offset + 4 * lane)[0]:08X}"
            for lane in range(4)) for offset in case["rows"]]
        kind = ("assemble" if case["function"] == 0x822A1E80
                else f"rotate:0x{case['function']:08X}")
        angles = ",".join(f"{name}={value!r}"
                          for name, value in sorted(case["floats"].items()))
        seed = "sentinel" if case["seed_rows"] else "identity"
        rows.append("\t".join([case["name"], kind, seed, angles, *basis]))

    arguments.output.write_text(HEADER + "\n" + "\n".join(rows) + "\n",
                                encoding="utf-8")
    print(f"wrote {arguments.output} vectors={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
