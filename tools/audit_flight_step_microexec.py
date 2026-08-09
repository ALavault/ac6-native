#!/usr/bin/env python3
"""Run the retail flight position integrator and compare it against the port.

`include/ac6/retail_flight_step.h` derives the integration by reading
0x82303558..0x82303694. Derived and tested is not verified: the tests check the
port against the rules I wrote down, and cycle 1372 already found one of those
rules backwards (`!(x >= 10)` instead of `x < 10`) by reading the branch
encoding, not by running anything. This closes the gap the plan names --
no flight behaviour enters the contract without a `microexec` differential.

WHERE EXECUTION STARTS, and why it is not the function's entry.

  0x82303110 is 359 instructions. Entering at the top would run a prologue that
  spills vector registers, a long stretch this cycle has not read, and calls out
  to 0x82380638 and others. None of that is the behaviour under test.

  Execution starts at **0x82303558**, the instruction that loads the model's
  rate scale, with every live register seeded. That is a smaller claim than
  "the function does this" and it is the claim the port actually makes: the
  header documents 0x82303558..0x82303694 and nothing above it.

WHY THE VECTOR BLOCK IS NOT EXERCISED, stated rather than hidden.

  0x823035EC..0x82303660 normalises the scaled rates with vrsqrtefp, vmsum3fp128
  and vupkd3d128 -- VMX128, two register files, the territory that cost this
  campaign thirteen cycles of instrument repair. It is guarded:

      if |s64| < f11 and |s68| < f11 and |s72| < f11:  r11 = 1
      bne cr6, 0x82303670                              -> skip the normalise

  Seeding **f11 huge** makes all three components "below the epsilon", the branch
  is taken, and the run is pure scalar -- exactly what cycle 1306 named as what
  this instrument can certify. The port does not implement the normalise either,
  and the header says so. This audit therefore covers the integration and the
  floor, and makes NO claim about the direction output at [model+128/132/136].

THE POSITION BLOCK IS READ AND WRITTEN, so it cannot be poison-detected: write
detection needs a poison fill and a block that must be pre-filled cannot have
one. It is seeded with `bytes:` and read back with `dump`, which emits both
poison passes separately so a reader can see the result does not depend on the
fill rather than take it on trust.

    python3 tools/audit_flight_step_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_flight_step_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

START = 0x82303558          # lfs f0,32(r31)
MODEL = 0xB8000000          # r31, the flight model; only +32 is read here
POSITION = 0xB4000000       # r5/r30, the block at [model+112] + 96
POSITION_SIZE = 0x50        # 64/68/72 are inside; 0x50 keeps the dump aligned

RATE_TO_STEP = struct.unpack(">f", struct.pack(">I", 0x3E8E38E4))[0]
MID_FLOOR = 10.0

# The epsilon seeded into f11. Any finite component compares below it, so the
# vector normalise is always skipped. It is not retail's value -- retail's f11
# was not read -- and picking it is a statement about coverage, not about the
# game.
EPSILON_SEED = 1.0e30


def f32(value: float) -> float:
    """Round to single, because every retail step is single precision."""
    return struct.unpack(">f", struct.pack(">f", value))[0]


def fmaf(a: float, b: float, c: float) -> float:
    """One rounding, like fmadds and fnmsubs. math.fma is exact-then-round."""
    return f32(math.fma(a, b, c))


def expected(position: tuple[float, float, float],
             rates: tuple[float, float, float],
             rate_scale: float, bias_a: float, bias_b: float,
             step: float) -> tuple[float, float, float]:
    """The port's rule, restated here so the two sides are independent code.

    Deliberately NOT a call into the C++: an audit that invoked the port would
    compare the port with itself. This is the same arithmetic written a second
    time from the listing, and if the two disagree that is the finding.
    """
    # ROUND EVERY INPUT FIRST, and this is not a formality. The spec seeds the
    # emulator with `f:` + repr(f32(value)) -- the single-precision value -- while
    # a Python literal like 13.7 is a double. Cycle 1373's first differential
    # reported two one-ulp mismatches on exactly the cases with long decimals,
    # and all four candidate rounding models (single/double x fused/unfused)
    # reproduced retail. The rule was never in question: the two sides were
    # computing from DIFFERENT INPUTS. Rounding here makes the oracle read the
    # same numbers the emulator was given.
    at64, at68, at72 = (f32(value) for value in position)
    rates = tuple(f32(value) for value in rates)
    rate_scale = f32(rate_scale)
    bias_a = f32(bias_a)
    bias_b = f32(bias_b)
    step = f32(step)
    s64 = f32(rate_scale * rates[0])
    s68 = f32(rate_scale * rates[1])
    s72 = f32(rate_scale * rates[2])
    s68 = fmaf(-bias_a, bias_b, s68)               # fnmsubs f10,f25,f24,f10
    at72 = fmaf(f32(s72 * step), RATE_TO_STEP, at72)
    at68 = fmaf(f32(s68 * step), RATE_TO_STEP, at68)
    at64 = fmaf(f32(s64 * step), RATE_TO_STEP, at64)
    if at68 < MID_FLOOR:                            # bge is taken at equality
        at68 = MID_FLOOR
    return (at64, at68, at72)


# (position, rates, rate_scale, bias_a, bias_b, step, why this case exists)
CASES = [
    ((0.0, 1000.0, 0.0), (0.0, 0.0, 0.0), 1.0, 0.0, 0.0, 0.0,
     "everything idle: nothing may move"),
    ((100.0, 2000.0, -50.0), (36.0, 72.0, 108.0), 1.0, 0.0, 0.0, 1.0,
     "distinct rate per axis: a crossed component shows here"),
    ((0.0, 4111.3, 0.0), (13.7, 907.3, 1.7), 1.1, 0.7, 1.3, 0.0166666668,
     "full mantissas: this is where fused and unfused disagree"),
    ((0.0, 10.0, 0.0), (0.0, 0.0, 0.0), 1.0, 0.0, 0.0, 1.0,
     "exactly the floor: bge is taken, so 10.0 is not rewritten"),
    ((0.0, 9.999999, 0.0), (0.0, 0.0, 0.0), 1.0, 0.0, 0.0, 1.0,
     "just below the floor: lifted to 10.0"),
    ((0.0, 5.0, 0.0), (0.0, -1000.0, 0.0), 1.0, 0.0, 0.0, 1.0,
     "driven below the floor by its own rate"),
    ((-1.0, 3000.0, -1.0), (0.0, 0.0, 0.0), 1.0, 0.0, 0.0, 1.0,
     "at64 and at72 below 10 with no motion: the floor must NOT touch them"),
    ((0.0, 2500.0, 0.0), (100.0, 100.0, 100.0), 1.0, 2.0, 3.0, 1.0,
     "equal rates, non-zero bias: only the middle may differ"),
    ((7.5, 800.25, -3.25), (-13.7, -3.7, 0.27), -1.3, -0.7, 0.9, -0.05,
     "negative everything, including the step"),
    ((1234.5677, 4321.7, -9.3), (0.33333331, 907.3, 1.7), 0.9, 0.1, 0.1,
     0.0033333334, "the sweep's own shape, one point of it"),
]


def name_of(index: int) -> str:
    return f"flight-step-{index}"


SPEC = """\
# Generated by tools/audit_flight_step_microexec.py --emit.
# {why}
#
# Entry is 0x82303558, not the function head: see the tool's docstring.
# f11 is seeded huge so the VMX normalise at 0x823035EC is always skipped.

function {start:#010x}
case flight-step:{name}
steps 46

region model {model:#010x} bytes:{model_bytes}
region pos   {position:#010x} bytes:{position_bytes}
region stack 0xC0000000 zero:0x2000

sp 0xC0001000
gpr r31 model
gpr r30 pos
gpr r0 0
fpr f12 {rate64}
fpr f10 {rate68}
fpr f9  {rate72}
fpr f31 {step}
fpr f25 {bias_a}
fpr f24 {bias_b}
fpr f11 {epsilon}
fpr f26 0x00000000
dump pos
"""


def word(value: float) -> str:
    """A seed for an FPR, and it must be `f:` rather than a bit pattern.

    PowerPC FPRs are 64 bits. The harness's bare-hex form writes the token into
    the register verbatim, so a 32-bit single pattern like 0x415B3333 lands as
    the DOUBLE 0x00000000415B3333 -- a denormal near 5.4e-315, not 13.7. Cycle
    1373's first run seeded all seven floats that way: every comparison went the
    wrong way, the vector block ran instead of being skipped, and the position
    came back untouched. The `f:` form parses a decimal and calls
    doubleToRawLongBits, which is what a double register needs.

    `repr` of a Python float is the shortest decimal that round-trips the
    double, and `value` here is already exactly representable as a single, so
    the double the emulator gets equals the single the port uses, bit for bit.
    """
    return "f:" + repr(f32(value))


def model_bytes(rate_scale: float) -> str:
    """0x24 bytes, with the rate scale at +32. Nothing else here is read."""
    blob = bytearray(0x24)
    struct.pack_into(">f", blob, 32, rate_scale)
    return blob.hex()


def position_bytes(position: tuple[float, float, float]) -> str:
    blob = bytearray(POSITION_SIZE)
    struct.pack_into(">f", blob, 64, position[0])
    struct.pack_into(">f", blob, 68, position[1])
    struct.pack_into(">f", blob, 72, position[2])
    return blob.hex()


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for index, case in enumerate(CASES):
        position, rates, scale, bias_a, bias_b, step, why = case
        spec = workdir / "specs" / f"{name_of(index)}.spec"
        spec.write_text(SPEC.format(
            why=why, start=START, name=name_of(index),
            model=MODEL, model_bytes=model_bytes(scale),
            position=POSITION, position_bytes=position_bytes(position),
            rate64=word(rates[0]), rate68=word(rates[1]), rate72=word(rates[2]),
            step=word(step), bias_a=word(bias_a), bias_b=word(bias_b),
            epsilon=word(EPSILON_SEED)), encoding="utf-8")
        manifest.append(f"{spec.resolve()} "
                        f"{(workdir / 'out' / (name_of(index) + '.json')).resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(manifest)} manifest={workdir / 'manifest'}")
    return 0


def dumped(document: dict) -> tuple[bytes, bytes]:
    for entry in document.get("region_dumps", []):
        if entry["name"] == "pos":
            return (bytes.fromhex(entry["after_hex"]),
                    bytes.fromhex(entry["after_hex_b"]))
    raise KeyError("the run emitted no dump of the position region")


def check(workdir: Path, report: Path | None) -> int:
    passed, failures, compared = 0, [], 0
    lines = []
    for index, case in enumerate(CASES):
        position, rates, scale, bias_a, bias_b, step, why = case
        path = workdir / "out" / f"{name_of(index)}.json"
        if not path.exists():
            failures.append(f"{name_of(index)}: no snapshot")
            continue
        document = json.loads(path.read_text(encoding="utf-8"))
        # 0x82303558..0x823035E8 is 37 instructions, the branch lands on
        # 0x82303670, and 0x82303670..0x82303690 is 9 more: 46 at most, one
        # fewer when the floor is not applied. `steps 46` therefore covers the
        # whole window and stops before the epilogue -- 50 did not, and the
        # callee guard below is what said so rather than a wrong number passing.
        #
        # The window ends four instructions before the epilogue's `bl`, so the
        # run is stopped by `steps 46` rather than by returning: entering at
        # 0x82303558 means the epilogue would restore LR from a zeroed stack and
        # branch to 0. `step_limit` is therefore the CORRECT exit here, and a
        # `return` would mean execution left the window early.
        if document["exit"]["kind"] != "step_limit":
            failures.append(f"{name_of(index)}: exit {document['exit']}")
            continue
        if document["provenance"]["callee_entries"] != 0:
            failures.append(f"{name_of(index)}: entered "
                            f"{document['provenance']['callee_entries']} callee(s); "
                            "the scalar window calls nothing")
            continue
        try:
            after_a, after_b = dumped(document)
        except KeyError as problem:
            failures.append(f"{name_of(index)}: {problem}")
            continue
        # The two poison passes must agree, or the result depended on the fill
        # and the seeding was incomplete. This is the check the `dump` directive
        # emits two passes for.
        if after_a != after_b:
            failures.append(f"{name_of(index)}: the two poison passes disagree")
            continue
        want = expected(position, rates, scale, bias_a, bias_b, step)
        got = struct.unpack(">fff", after_a[64:76])
        bad = []
        for offset, (g, w) in zip((64, 68, 72), zip(got, want)):
            compared += 1
            if struct.pack(">f", g) != struct.pack(">f", w):
                bad.append(f"+{offset}: retail {g!r} port {w!r}")
        if bad:
            failures.append(f"{name_of(index)}: " + "; ".join(bad))
        else:
            passed += 1
        lines.append(f"{name_of(index)}\t{'ok' if not bad else 'MISMATCH'}\t"
                     f"{got[0]!r}\t{got[1]!r}\t{got[2]!r}\t{why}")
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "# 0x82303110 from 0x82303558, micro-executed, against\n"
            "# ac6::retail::integrate_flight_position.\n"
            "# case\tverdict\tat64\tat68\tat72\twhy the case exists\n"
            + "\n".join(lines) + "\n", encoding="utf-8")
    for problem in failures:
        print("  FAIL  " + problem)
    print(f"flight_step_microexec={'pass' if not failures else 'fail'} "
          f"cases={len(CASES)} passed={passed} values_compared={compared}")
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
