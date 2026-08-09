#!/usr/bin/env python3
"""Run the three rotation angles of 0x82302C88 and compare them to the port.

`include/ac6/retail_flight_orientation.h` derives the angles slot 32 hands to
the three rotations. Two of the three are computed inside 0x82302C88 itself and
the third inside its helper 0x82302B78, and none of them is ever stored to
memory -- each goes straight into f1 and into a rotation that is VMX128. So the
usual comparison, "run it and diff the object", does not apply.

HOW EACH ONE IS OBSERVED, and the two techniques are different:

  ROW 0 is clean. 0x82302B78 computes the angle, calls 0x820A99F8 with it, and
  RETURNS. Stub the rotation and f1 still holds the angle at the `blr`, so the
  case runs the whole function and captures the result. Nothing is truncated.

  ROWS 1 AND 2 are mid-function values in 0x82302C88, so each case stops with
  `steps` at the instruction that would consume them. That count is PATH
  DEPENDENT -- the row-1 clamp has three exits of different lengths -- so the
  emitter computes it from the same branch decision the expectation makes, and
  the check asserts `stubbed_calls` is exactly the number of calls that path
  should have reached. A miscount changes that number and fails the case loudly
  rather than capturing the wrong register.

WHAT THIS DOES NOT COVER. The rotations themselves (already ported and contracted
under A3.1), the Euler extraction, and the case where bit 4 of [model+332] scales
the row-0 divisor -- that calls 0x822A6400, which this audit stubs, so the
divisor is undefined and the case is reported and NOT compared.

    python3 tools/audit_flight_orientation_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_flight_orientation_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ROW0_HELPER = 0x82302B78
SLOT32 = 0x82302C88
MODEL = 0xB8000000
POSITION = 0xB4000000

DEG = 0.01745329238474369      # 0x82069BF4
ROW1_SCALE = 0.06666667014360428   # 1/15, 0x82007D5C
ROW2_SCALE = 0.6666666865348816    # 2/3,  0x82069C1C
ROW0_DIVISOR = 7.0             # 0x82069D1C

# Indices into 0x82302C88, counted from its first instruction.
ROW1_CONSUMER = (0x82302CF4 - SLOT32) // 4     # bl 0x820A9B30
ROW2_CONSUMER = (0x82302D38 - SLOT32) // 4     # bl 0x82211828
ROW1_CLAMP_TEST = (0x82302CD0 - SLOT32) // 4   # bgt
ROW1_STORE = (0x82302CE0 - SLOT32) // 4        # fmr f0,f13


def f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


def row1_angle(limit: float, axis: float, step: float):
    """Returns (angle, steps to reach the consumer)."""
    limit, axis, step = f32(limit), f32(axis), f32(step)
    value = f32(f32(limit * step) * ROW1_SCALE)
    value = f32(value * axis)
    if value > limit:                       # bgt jumps forward over three
        value, taken = limit, ROW1_CONSUMER - 3
    elif value >= -limit:                   # bge jumps over one
        taken = ROW1_CONSUMER - 1
    else:
        value, taken = -limit, ROW1_CONSUMER
    return f32(value * f32(DEG)), taken


def row2_angle(limit1: float, axis1: float, limit2: float, axis2: float,
               step: float):
    """Returns (angle, steps to reach the consumer).

    The path to the row-2 consumer runs through the row-1 clamp, so its length
    depends on BOTH branch decisions. Getting that wrong does not corrupt the
    answer silently: the case would stop somewhere else and the reached-call
    count would not be 2.
    """
    limit2, axis2, step = f32(limit2), f32(axis2), f32(step)
    value = f32(f32(f32(limit2 * step) * ROW2_SCALE) * axis2)
    clamped = value > limit2
    if clamped:
        value = limit2
    _, row1_taken = row1_angle(limit1, axis1, step)
    row1_skip = ROW1_CONSUMER - row1_taken
    # `ble` at 0x82302D28 skips the single `fmr` when the value is not clamped.
    #
    # AND EACH STUBBED CALL COSTS TWO STEPS, NOT ONE. The harness keys stubs on
    # the CALLEE's entry address: the `bl` executes normally and counts one, then
    # the stub fires at the callee's first instruction and counts another,
    # setting PC back to LR. The row-2 window passes two stubbed calls, so it
    # needs two extra steps. The first version of this tool assumed one step per
    # call, stopped eleven instructions early, and captured f1 still holding the
    # STEP -- retail "returned" 0.016666667, 1000.0 and 100.377 for the three
    # different step values, which is what made it obvious.
    return (f32(value * f32(DEG)),
            ROW2_CONSUMER - row1_skip - (0 if clamped else 1) + 2)


def row0_angle(limit: float, axis: float, step: float, divisor: float) -> float:
    limit, axis, step = f32(limit), f32(axis), f32(step)
    value = f32(1.0 / divisor)
    value = f32(value * limit)
    value = f32(value * step)
    value = f32(value * axis)
    if value > limit:
        value = limit
    return f32(value * f32(DEG))


# (limit1248, limit1252, limit1256, axis304, axis308, axis312, step, flags, why)
CASES = [
    (5.0, 1.4, 5.4, 0.0, 0.0, 0.0, 0.016666668, 0, "everything idle"),
    (5.0, 1.4, 5.4, 1.0, 1.0, 1.0, 0.016666668, 0, "one frame at full deflection"),
    (5.0, 1.4, 5.4, -1.0, -1.0, -1.0, 0.016666668, 0, "and the same negative"),
    (5.0, 1.4, 5.4, 1.0, 1.0, 1.0, 1000.0, 0, "every angle saturates high"),
    (5.0, 1.4, 5.4, -1.0, -1.0, -1.0, 1000.0, 0,
     "row 1 saturates low; rows 0 and 2 have NO lower clamp and run past it"),
    (5.0, 1.4, 5.4, 0.37777779, 0.37777779, 0.37777779, 0.016666668, 0,
     "full mantissas"),
    (1.4, 5.4, 5.0, -3.7, 0.5, -3.7, 100.37777, 0,
     "limits permuted, so a port that crossed two limits fails here"),
    (5.0, 1.4, 5.4, 0.5, 0.5, 0.5, 0.016666668, 0x10,
     "bit 4 set: 0x822A6400 is stubbed, so row 0 is reported and not compared"),
]


def model_bytes(case) -> str:
    l0, l1, l2, a0, a1, a2, step, flags, _ = case
    blob = bytearray(0x600)
    struct.pack_into(">f", blob, 1248, f32(l0))
    struct.pack_into(">f", blob, 1252, f32(l1))
    struct.pack_into(">f", blob, 1256, f32(l2))
    struct.pack_into(">f", blob, 304, f32(a0))
    struct.pack_into(">f", blob, 308, f32(a1))
    struct.pack_into(">f", blob, 312, f32(a2))
    struct.pack_into(">I", blob, 332, flags)
    return blob.hex()


HEAD = """\
# Generated by tools/audit_flight_orientation_microexec.py --emit.
# {why}

function {start:#010x}
case flight-orientation:{name}
{steps}
region model {model:#010x} bytes:{model_bytes}
region pos   {position:#010x} zero:0x80
region stack 0xC0000000 zero:0x2000

sp 0xC0001000
gpr r3 model
gpr r5 pos
gpr r0 0
fpr f1 f:{step!r}
{stubs}capture fpr:f1
"""

ROW0_STUBS = ("stub 0x822a6400 the divisor scaler, outside this behaviour\n"
              "stub 0x820a99f8 the rotation, already ported as rotate_820A99F8\n")
SLOT32_STUBS = ("stub 0x820a9b30 rotation, ported as rotate_820A9B30\n"
                "stub 0x82302b78 the row-0 helper, measured separately\n"
                "stub 0x82211828 rotation, ported as rotate_82211828\n")


def specs_for(index, case):
    l0, l1, l2, a0, a1, a2, step, flags, why = case
    out = []
    out.append(("row0", ROW0_HELPER, "", ROW0_STUBS,
                f"row 0 of {why}"))
    _, row1_steps = row1_angle(l1, a1, step)
    out.append(("row1", SLOT32, f"steps {row1_steps}\n", SLOT32_STUBS,
                f"row 1 of {why}"))
    _, row2_steps = row2_angle(l1, a1, l2, a2, step)
    out.append(("row2", SLOT32, f"steps {row2_steps}\n", SLOT32_STUBS,
                f"row 2 of {why}"))
    return out


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for index, case in enumerate(CASES):
        for kind, start, steps, stubs, why in specs_for(index, case):
            name = f"{kind}-{index}"
            spec = workdir / "specs" / f"{name}.spec"
            spec.write_text(HEAD.format(
                why=why, start=start, name=name, steps=steps, model=MODEL,
                model_bytes=model_bytes(case), position=POSITION,
                step=f32(case[6]), stubs=stubs), encoding="utf-8")
            manifest.append(f"{spec.resolve()} "
                            f"{(workdir / 'out' / (name + '.json')).resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(manifest)} manifest={workdir / 'manifest'}")
    return 0


def captured(document) -> float:
    raw = document["registers"]["f1"]
    return f32(struct.unpack(">d", struct.pack(">Q", int(raw, 16)))[0])


def check(workdir: Path, report: Path | None) -> int:
    failures, compared, lines = [], 0, []
    for index, case in enumerate(CASES):
        l0, l1, l2, a0, a1, a2, step, flags, why = case
        for kind in ("row0", "row1", "row2"):
            name = f"{kind}-{index}"
            path = workdir / "out" / f"{name}.json"
            if not path.exists():
                failures.append(f"{name}: no snapshot")
                continue
            document = json.loads(path.read_text(encoding="utf-8"))
            provenance = document["provenance"]
            if provenance["callee_entries"] != 0:
                failures.append(f"{name}: entered a callee; every call is stubbed")
                continue
            got = captured(document)
            if kind == "row0":
                if document["exit"]["kind"] != "return":
                    failures.append(f"{name}: exit {document['exit']}")
                    continue
                if flags & 0x10:
                    lines.append(f"{name}\tnot-compared\t{got!r}\t{why}")
                    continue
                want = row0_angle(l0, a0, step, ROW0_DIVISOR)
            else:
                if document["exit"]["kind"] != "step_limit":
                    failures.append(f"{name}: exit {document['exit']}, expected "
                                    "step_limit -- the step count missed the "
                                    "instruction that consumes the angle")
                    continue
                # The harness records intercepted calls under `calls`, not in
                # `provenance` -- the first version of this checker looked for a
                # `stubbed_calls` key that only exists in the console line, and
                # died with a KeyError instead of silently passing. A missing
                # guard that crashes is the good failure mode.
                expected_calls = 0 if kind == "row1" else 2
                if len(document["calls"]) != expected_calls:
                    failures.append(f"{name}: reached {len(document['calls'])} "
                                    f"call(s), expected {expected_calls}; the "
                                    "step count landed in the wrong place")
                    continue
                if kind == "row1":
                    want, _ = row1_angle(l1, a1, step)
                else:
                    want, _ = row2_angle(l1, a1, l2, a2, step)
            compared += 1
            if struct.pack(">f", got) != struct.pack(">f", want):
                failures.append(f"{name}: retail {got!r} port {want!r}")
                lines.append(f"{name}\tMISMATCH\t{got!r}\t{why}")
            else:
                lines.append(f"{name}\tok\t{got!r}\t{why}")
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "# The rotation angles of 0x82302C88 and its helper 0x82302B78,\n"
            "# micro-executed, against ac6::retail::flight_rotation_angles.\n"
            "# Row 0 runs the helper whole; row 1 stops at the instruction that\n"
            "# consumes it, with the reached-call count asserted zero.\n"
            "# case\tverdict\tretail_radians\twhy the case exists\n"
            + "\n".join(lines) + "\n", encoding="utf-8")
    for problem in failures:
        print("  FAIL  " + problem)
    print(f"flight_orientation_microexec={'pass' if not failures else 'fail'} "
          f"cases={len(CASES)} values_compared={compared}")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    if arguments.emit == arguments.check:
        print("choose exactly one of --emit and --check")
        return 2
    return emit(arguments.workdir) if arguments.emit else check(
        arguments.workdir, arguments.report)


if __name__ == "__main__":
    raise SystemExit(main())
