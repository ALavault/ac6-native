#!/usr/bin/env python3
"""Turn the two micro-execution sweeps into a committed vector table.

The retail side of the input-record differential is 321 runs of 0x821CAA50, and
running them needs Ghidra. `ctest` must not. So the runs are reduced here to one
line each -- the fields the native port reads, and the record bytes retail left
behind -- and tests/retail_input_record_differential_tests.cpp reads the table.

The record bytes come from the harness's 0x00 poison pass, which is not a
convenience: 0x821CA908 clears record +0x00..+0x83 every frame, so a zeroed
record IS the consumer's precondition and the 0xCD pass is the artificial one.

    python3 tools/emit_input_record_vectors.py --axis-workdir A --button-workdir B \
        --output analysis/input-path/input-record-vectors.tsv
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from audit_input_axis_sweep_microexec import cases as axis_cases, read_record  # noqa: E402
from audit_input_button_map_microexec import (  # noqa: E402
    cases as button_cases, clean_memory, flag_word, float_slots,
)
from audit_input_consumer_microexec import (  # noqa: E402
    CONTROLLER_0, HALVES, SERVICE,
)

HEADER = """\
# Retail vectors for the input record, produced by micro-executing 0x821CAA50.
# Every row is one run: the fields the native port reads, and the record bytes
# retail left behind, read from the harness's 0x00 poison pass -- which is also
# the state 0x821CA908 clears the record to before the consumer runs.
#
# Committed so tests/retail_input_record_differential_tests.cpp can run inside
# ctest without Ghidra. Regenerate with tools/emit_input_record_vectors.py.
#
# The `pressed` and `released` sweeps are NOT here: they change no record byte,
# so they are a negative result and belong in the report, not in a vector table
# that exists to pin values.
#
# columns: case held scalar14 scalar15 halves flags slots"""


def bits(value: float) -> int:
    return struct.unpack(">I", struct.pack(">f", value))[0]


def row(name, held, scalar14, scalar15, halves, slots, flags) -> str:
    return "\t".join([
        name, f"0x{held:08X}", f"0x{scalar14:04X}", f"0x{scalar15:04X}",
        ",".join(f"{key}=0x{value:04X}" for key, value in sorted(halves.items())) or "-",
        f"0x{flags:08X}",
        ",".join(f"0x{offset:02X}=0x{bits(value):08X}"
                 for offset, value in sorted(slots.items())),
    ])


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--axis-workdir", type=Path, required=True)
    parser.add_argument("--button-workdir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)

    rows: list[str] = []
    for case in axis_cases():
        if case["half"] is None:
            continue
        document = json.loads(
            (arguments.axis_workdir / "out" / f"{case['name']}.json").read_text())
        slots, flags = read_record(document)
        rows.append(row(f"axis-{case['name']}", 0, 0, 0,
                        {case["half"]: case["value"]}, slots, flags))

    for case in button_cases():
        if case["sweep"] in ("pressed", "released"):
            continue
        document = json.loads(
            (arguments.button_workdir / "out" / f"{case['name']}.json").read_text())
        memory = clean_memory(document)
        slots, flags = float_slots(memory), flag_word(memory)
        held = (1 << case["bit"]) if (case["sweep"] == "held"
                                      and case["bit"] is not None) else 0
        scalar14 = scalar15 = 0
        halves: dict[str, int] = {}
        if case["name"].startswith("field-"):
            # Recovered from the built blob, not parsed out of the case name: the
            # name is a label and the blob is what the emulator actually ran.
            blob = bytes.fromhex(case["service_blob"])
            base = CONTROLLER_0 - SERVICE
            scalar14 = struct.unpack_from(">H", blob, base + 0x38)[0]
            scalar15 = struct.unpack_from(">H", blob, base + 0x3A)[0]
            for half, offset in HALVES.items():
                value = struct.unpack_from(">H", blob, base + offset)[0]
                if value:
                    halves[half] = value
        rows.append(row(f"btn-{case['name']}", held, scalar14, scalar15,
                        halves, slots, flags))

    arguments.output.write_text(HEADER + "\n" + "\n".join(rows) + "\n",
                                encoding="utf-8")
    print(f"wrote {arguments.output} vectors={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
