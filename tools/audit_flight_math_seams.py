#!/usr/bin/env python3
"""Measure the two scalar math routines the orientation update calls.

WHY THESE TWO. 0x82302C88 -- slot 32 of the vtable 0x8200F270, the third pure
virtual the step 0x82283898 calls -- ends by extracting three angles:

    f1 = [pos+52]; clamp to [-1, +1]; bl 0x82380570; f31 = -frsp(f1)
    bl 0x820B0B60 -> lfs f1,20(r3); lfs f2,36(r3); b 0x820936E8
    bl 0x82093808 -> lfs f1,48(r3); lfs f2,56(r3); b 0x820936E8
    [model+16] = f31 ; [model+20] = ... ; [model+24] = ...

0x82380570 clamps its argument to asin's domain and uses `fsqrt`; 0x820936E8
takes two arguments and guards both against 2**-16. Those are the SHAPES of asin
and atan2, and a shape is not an identity. This measures them.

WHY IT MATTERS RATHER THAN BEING TRIVIA. `XMScalarSinCos` at 0x8209CB70 was
identified the same way at cycle 1307 and became a certified seam that
retail_transform.cpp is written against. A ported orientation update needs the
same for these two: if 0x82380570 is asinf, `std::asin` is a legitimate
substitute and the port stays readable; if it is a table approximation that
differs in the last ulp, the port has to carry the table.

The verdict this tool prints is therefore three-valued -- identical, within N
ulp, or different -- and it reports the worst case it saw rather than an average.

    python3 tools/audit_flight_math_seams.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_flight_math_seams.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

ASIN_LIKE = 0x82380570
ATAN2_LIKE = 0x820936E8
SINCOS = 0x8209CB70    # XMScalarSinCos(float* sin /*r3*/, float* cos /*r4*/, f1)

# WHAT CYCLE 1411 ADDED, and why the tool was not enough as it stood.
#
# Cycle 1410 refused a port because std::cos disagreed with retail's own cosine
# at one argument out of 96 -- a disagreement that eight chosen points had not
# found, and could not have. This tool had the same weakness twice over: 42
# chosen points and NO SWEEP, and a pass condition of "within 2 ulp" under a
# contract claim of "identical at 0 ulp". The chosen points stay, because the
# endpoints and the guard edges are where an approximation breaks and a uniform
# sweep would miss them. The sweeps are added beside them, and the tolerance is
# gone: the verdict is now 0 ulp or a failure.
#
# XMScalarSinCos is added because it is the largest unswept substitution in the
# product. retail_transform.cpp calls std::sin and std::cos where retail calls
# 0x8209CB70, its header names that as a seam, and cycle 1307 certified it at
# FOURTEEN ANGLES -- which on cycle 1410's evidence is fourteen points and not a
# domain. Every rotation in the flight chain goes through it.
SWEEP = 192

# WHICH SEAMS THE PRODUCT MAY SUBSTITUTE, declared rather than tolerated.
#
# A pass condition of "within N ulp" is a tolerance wearing a gate's clothes --
# the previous version passed at 2 ulp under a contract claim of 0. So each seam
# now carries an explicit verdict:
#
#   True  -- the product calls the library function here, and that is admissible
#            ONLY at 0 ulp over the whole sweep.  Anything else fails.
#   False -- the substitution is known NOT to be faithful.  The tool does not
#            fail on it, because failing every run would make the gate useless;
#            it asserts the measured gap has not grown past the recorded
#            baseline, and prints the baseline every time so it cannot be
#            forgotten.  A number in the output is harder to ignore than a
#            comment in a header.
SUBSTITUTABLE = {"asin": True, "atan2": True, "sincos": False}

# Measured at cycle 1411 over 412 values: 170 exact, 182 within 2 ulp, 60 worse,
# worst absolute error 3.20375e-07 (sin, near -pi) and 2.98023e-07 (cos).
# The large ulp figures all sit where the true value is near zero, so the ulp
# metric exaggerates them and the absolute error is the one that means anything.
SINCOS_BASELINE_ABS = 3.3e-07


def f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


def bits_of(value: float) -> int:
    return struct.unpack(">I", struct.pack(">f", value))[0]


def ulp_gap(a: float, b: float) -> int:
    """Distance in representable float32 steps. Ordered so sign changes count."""
    if math.isnan(a) or math.isnan(b):
        return -1 if math.isnan(a) != math.isnan(b) else 0

    def ordered(value: float) -> int:
        raw = bits_of(value)
        return raw if raw < 0x80000000 else -(raw - 0x80000000)

    return abs(ordered(f32(a)) - ordered(f32(b)))


# asin's whole domain plus the exact endpoints, where an approximation is most
# likely to disagree and where a naive table walks off its end.
ASIN_ARGS = [-1.0, -0.9999999, -0.99, -0.75, -0.5, -0.25, -0.0625, -1.0e-4,
             -0.0, 0.0, 1.0e-4, 0.0625, 0.25, 0.5, 0.7071068, 0.75, 0.9,
             0.99, 0.9999999, 1.0]
ASIN_ARGS += [f32(-1.0 + 2.0 * index / (SWEEP - 1)) for index in range(SWEEP)]

# The angles a rotation can carry. The command setters wrap to [-pi, pi], so
# that is the reachable domain and the sweep covers all of it; the named angles
# are the quadrant boundaries and the reduction's own seams.
PI = 3.1415927410125732
SINCOS_ARGS = [0.0, -0.0, PI, -PI, PI / 2, -PI / 2, PI / 4, -PI / 4,
               1.0, -1.0, 1.5707963705062866, 0.7853981852531433,
               2.3561944961547852, 3.1415926535897931]
SINCOS_ARGS += [f32(-PI + 2.0 * PI * index / (SWEEP - 1)) for index in range(SWEEP)]

# THE GUARD, and it is not cosmetic. 0x820936E8 opens with
#
#     fabs f13,f1 ; fcmpu cr6,f13,2**-16 ; bge -> compute
#     fabs f13,f2 ; fcmpu cr6,f13,2**-16 ; blt -> return 0
#
# so when BOTH arguments are below 2**-16 in absolute value it returns zero
# instead of an angle. std::atan2(1e-6, 1e-6) is pi/4; retail is 0. A port that
# substituted std::atan2 unguarded would put a 45-degree error into the
# orientation on exactly the frames where the aircraft is level -- the most
# common frame in the game, and the one least likely to be noticed by eye.
#
# The first run of this tool reported "within 1061752795 ulp" and that number is
# the point: it is not a rounding disagreement, it is a different function on a
# subdomain, and an average or a tolerance would have hidden it.
ATAN2_GUARD = 1.52587890625e-05          # 0x82069C2C, the same word f11 uses

ATAN2_ARGS = [(0.0, 1.0), (1.0, 0.0), (0.0, -1.0), (-1.0, 0.0),
              (1.0, 1.0), (1.0, -1.0), (-1.0, 1.0), (-1.0, -1.0),
              (0.5, 2.0), (2.0, 0.5), (-0.5, 2.0), (-2.0, -0.5),
              (3.7, -9.3), (-907.3, 13.7), (0.33333331, 1.7),
              # the guard: both below, one below, and either side of the edge
              (1.0e-6, 1.0e-6), (1.5e-5, 1.5e-5), (0.0, 0.0),
              (1.0e-6, 1.0), (1.0, 1.0e-6),
              (1.52587890625e-05, 1.52587890625e-05),
              (1.5258789e-05, 1.6e-05)]
# A sweep of the unit circle: every quadrant, and both signs of both arguments.
ATAN2_ARGS += [(f32(math.sin(-math.pi + 2.0 * math.pi * index / (SWEEP - 1))),
                f32(math.cos(-math.pi + 2.0 * math.pi * index / (SWEEP - 1))))
               for index in range(SWEEP)]


def guarded_atan2(a: float, b: float) -> float:
    """atan2 with retail's both-below-2**-16 short circuit."""
    if abs(f32(a)) < ATAN2_GUARD and abs(f32(b)) < ATAN2_GUARD:
        return 0.0
    return math.atan2(f32(a), f32(b))

SPEC_ONE = """\
# Generated by tools/audit_flight_math_seams.py --emit.
# {why}

function {start:#010x}
case math-seam:{name}
steps 200

region stack 0xC0000000 zero:0x2000

sp 0xC0001000
gpr r0 0
fpr f1 f:{a!r}
{extra}capture fpr:f1
"""

# XMScalarSinCos returns through POINTERS, not f1, so its cases need two output
# regions and a dump rather than a register capture.
#
# AND IT NEEDS `alias on`.  Without the register-file bridge this routine returns
# a correct cos and a first output that is NOT sin -- 1.0 at angle 0, where sine
# is 0.  That is cycle 1300's defect exactly, and cycle 1302 fixed it by copying
# each vector-register write to its alias.  The first version of these cases
# omitted the directive and reproduced cycle 1300's broken table to the digit,
# which is the strongest possible evidence that the directive is what is missing
# and not the routine that is wrong.
SPEC_SINCOS = """\
# Generated by tools/audit_flight_math_seams.py --emit.
# {why}

function {start:#010x}
case math-seam:{name}
steps 400

region out   0xB4000000 zero:0x10
region stack 0xC0000000 zero:0x2000

alias on
sp 0xC0001000
gpr r3 out
gpr r4 0xB4000008
gpr r0 0
fpr f1 f:{a!r}
dump out
"""


def name_of(kind: str, index: int) -> str:
    return f"{kind}-{index}"


def cases():
    built = []
    for index, value in enumerate(ASIN_ARGS):
        built.append({"kind": "asin", "name": name_of("asin", index),
                      "start": ASIN_LIKE, "a": f32(value), "b": None,
                      "want": math.asin(max(-1.0, min(1.0, f32(value)))),
                      "why": f"0x82380570 at {value!r}"})
    for index, (a, b) in enumerate(ATAN2_ARGS):
        built.append({"kind": "atan2", "name": name_of("atan2", index),
                      "start": ATAN2_LIKE, "a": f32(a), "b": f32(b),
                      "want": guarded_atan2(a, b),
                      "why": f"0x820936E8 at ({a!r}, {b!r})"})
    for index, value in enumerate(SINCOS_ARGS):
        built.append({"kind": "sincos", "name": name_of("sincos", index),
                      "start": SINCOS, "a": f32(value), "b": None,
                      "want": (math.sin(f32(value)), math.cos(f32(value))),
                      "why": f"0x8209CB70 at {value!r}"})
    return built


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for case in cases():
        extra = "" if case["b"] is None else f"fpr f2 f:{case['b']!r}\n"
        template = SPEC_SINCOS if case["kind"] == "sincos" else SPEC_ONE
        spec = workdir / "specs" / f"{case['name']}.spec"
        spec.write_text(template.format(why=case["why"], start=case["start"],
                                        name=case["name"], a=case["a"],
                                        extra=extra), encoding="utf-8")
        manifest.append(f"{spec.resolve()} "
                        f"{(workdir / 'out' / (case['name'] + '.json')).resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(manifest)} manifest={workdir / 'manifest'}")
    return 0


def check(workdir: Path, report: Path | None) -> int:
    lines, problems = [], []
    worst_abs = 0.0
    worst = {"asin": 0, "atan2": 0, "sincos": 0}
    counts = {"asin": 0, "atan2": 0, "sincos": 0}
    for case in cases():
        path = workdir / "out" / f"{case['name']}.json"
        if not path.exists():
            problems.append(f"{case['name']}: no snapshot")
            continue
        document = json.loads(path.read_text(encoding="utf-8"))
        if document["exit"]["kind"] != "return":
            problems.append(f"{case['name']}: exit {document['exit']}")
            continue
        if case["kind"] == "sincos":
            dumps = {d["name"]: d["after_hex"] for d in document["region_dumps"]}
            blob = bytes.fromhex(dumps["out"])
            got_sin = struct.unpack_from(">f", blob, 0)[0]
            got_cos = struct.unpack_from(">f", blob, 8)[0]
            want_sin, want_cos = (f32(v) for v in case["want"])
            for label, got, want in (("sin", got_sin, want_sin),
                                     ("cos", got_cos, want_cos)):
                gap = ulp_gap(got, want)
                worst["sincos"] = max(worst["sincos"], gap)
                worst_abs = max(worst_abs, abs(got - want))
                counts["sincos"] += 1
                lines.append(f"{case['name']}.{label}\t{got!r}\t{want!r}\t{gap}"
                             f"\t{case['why']}")
            continue
        raw = document.get("registers", {}).get("f1")
        if raw is None:
            problems.append(f"{case['name']}: f1 was not captured")
            continue
        # An FPR holds a double; these routines return a single-precision result
        # in it, so compare after rounding both sides to float.
        got = f32(struct.unpack(">d", struct.pack(">Q", int(raw, 16)))[0])
        want = f32(case["want"])
        gap = ulp_gap(got, want)
        worst[case["kind"]] = max(worst[case["kind"]], gap)
        counts[case["kind"]] += 1
        lines.append(f"{case['name']}\t{got!r}\t{want!r}\t{gap}\t{case['why']}")
    for kind in ("asin", "atan2", "sincos"):
        verdict = ("identical" if worst[kind] == 0
                   else f"within {worst[kind]} ulp")
        flag = "" if SUBSTITUTABLE[kind] else "   [DECLARED UNFAITHFUL]"
        print(f"  {kind:<6} cases={counts[kind]:<3} worst={worst[kind]} ulp"
              f"   -> {verdict}{flag}")
    print(f"  sincos worst absolute error {worst_abs:.6g} "
          f"(baseline {SINCOS_BASELINE_ABS:.6g}) -- "
          f"std::sin/std::cos are NOT 0x8209CB70 and the product knows it")
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "# 0x82380570 against asin and 0x820936E8 against atan2, both in\n"
            "# single precision. `ulp` is the distance in representable float32\n"
            "# steps; 0 means the same word.\n"
            "# case\tretail\tlibm\tulp\twhat was run\n"
            + "\n".join(lines) + "\n", encoding="utf-8")
    for problem in problems:
        print("  FAIL  " + problem)
    # NO TOLERANCE. The previous version passed at "within 2 ulp" under a
    # contract claim of "identical at 0 ulp"; a gate looser than the claim it
    # guards is not a gate. A substitution is admissible only if it is the same
    # word at every argument measured.
    ok = (not problems
          and all(worst[kind] == 0 for kind, may in SUBSTITUTABLE.items() if may)
          and worst_abs <= SINCOS_BASELINE_ABS)
    print(f"flight_math_seams={'pass' if ok else 'fail'} "
          f"cases={len(cases())} values={sum(counts.values())} "
          f"asin_worst={worst['asin']} atan2_worst={worst['atan2']} "
          f"sincos_worst={worst['sincos']}")
    return 0 if ok else 1


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
