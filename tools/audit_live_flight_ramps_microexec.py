#!/usr/bin/env python3
"""Run the live flight model's ramp update and compare it against the port.

0x82303E68 is slot 30 of the vtable 0x8200F310 -- the flight model cycle 1384
showed is the one that runs, as against 0x8200F270 whose instance nothing
addresses. It has no call and no vector instruction in 282 instructions, so it
executes end to end from its own entry.

WHAT IS COMPARED, and what is deliberately not. The function writes ten words:
+360, +364, +368, +372 and +376 from the ramp block at 0x82303EA4..0x82303FAC,
and +304, +308, +312, +1352 and +1356 from the three axis blocks after it.
`include/ac6/retail_live_flight_ramps.h` models the ramp block only, so this
audit compares those five and REPORTS the other five without comparing them.

That split is not a convenience: every store to r31 was listed in address order
and the five ramp offsets appear only before 0x82303FB0, so the block is a
complete unit rather than a prefix of one.

    python3 tools/audit_live_flight_ramps_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_live_flight_ramps_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

START = 0x82303E68
MODEL = 0xB8000000
VTABLE = 0xB9000000
MODEL_SIZE = 0x600

# The bypass path dispatches slot 38 (offset 152). Every case gets a synthetic
# vtable holding a REAL address there, stubbed, so the dispatch under test is a
# real one; without it the bypass case faults on `lwz r11,152(0)`, which is how
# the first run of this audit reported it.
SLOT38_STUB = 0x82282C50

RAMPS = (360, 364, 368, 372, 376)
AXES = (304, 308, 312, 1352, 1356)
INPUTS = (48, 52, 404, 952, 956)
GATE = 1224
FLAGS = 332
BYPASS = 1 << 7
CEILING = 1.0


def f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


def fmaf(a: float, b: float, c: float) -> float:
    return f32(math.fma(a, b, c))


def expected(state, reads, gate, flags, step):
    """The port's rule, restated from the listing. Inputs rounded to single
    first -- cycle 1373 lost a run to an oracle computing from doubles."""
    s = {k: f32(v) for k, v in state.items()}
    r = {k: f32(v) for k, v in reads.items()}
    step = f32(step)
    if not (flags & BYPASS):
        s[360] = fmaf(f32(r[48] - s[360]), f32(r[952] * step), s[360])
        s[364] = fmaf(f32(r[52] - s[364]), f32(r[956] * step), s[364])

        def secondary(value, primary, ceiling):
            if primary > r[404]:
                value = f32(value + step)
                if value > ceiling:
                    value = ceiling
            else:
                value = f32(value - step)
                if value < 0.0:
                    value = 0.0
            return value

        s[368] = secondary(s[368], s[360], CEILING)
        if gate == 0:
            s[368] = 0.0
        s[372] = secondary(s[372], s[364], CEILING)
    s[376] = f32(s[360] - s[364])
    return s


def case(name, why, step=0.016666668, flags=0, gate=1, **kwargs):
    state = {f: 0.0 for f in RAMPS}
    reads = {f: 0.0 for f in INPUTS}
    for key, value in kwargs.items():
        field = int(key[1:])
        (state if field in RAMPS else reads)[field] = value
    return {"name": name, "why": why, "step": step, "flags": flags,
            "gate": gate, "state": state, "reads": reads}


CASES = [
    case("idle", "everything zero"),
    case("lag-from-zero", "a lag takes a fraction of the gap",
         f48=1.0, f52=1.0, f952=3.0, f956=3.0),
    case("lag-at-target", "at the target the lag does not move",
         f360=1.0, f364=1.0, f48=1.0, f52=1.0, f952=3.0, f956=3.0),
    case("overshoot", "rate*step > 1 goes PAST the target and retail lets it",
         f48=1.0, f52=1.0, f952=3.0, f956=3.0, step=1.0),
    case("separate-commands", "cmd48 must not reach at364",
         f48=1.0, f52=0.0, f952=3.0, f956=3.0),
    case("secondary-up", "past the threshold both secondaries rise a whole step",
         f360=1.0, f364=1.0, f404=0.5),
    case("secondary-down", "below it they fall a whole step",
         f368=0.5, f372=0.5, f404=0.5),
    case("secondary-ceiling", "and stop at 1.0",
         f360=1.0, f364=1.0, f368=1.0, f372=1.0, f404=0.5),
    case("secondary-floor", "and at zero", f368=1.0e-9, f372=1.0e-9, f404=0.5),
    case("gate-clears-at368-only", "a zero gate byte clears at368 and not at372",
         gate=0, f360=1.0, f364=1.0, f368=0.5, f372=0.5, f404=0.5),
    case("bypass", "bit 7 skips the lags but at376 is still recomputed",
         flags=BYPASS, f360=0.75, f364=0.25, f368=0.5, f48=1.0, f952=3.0),
    case("full-mantissas", "long binary expansions everywhere",
         f360=0.37777779, f364=-0.633333, f368=0.13333334, f372=0.7333333,
         f48=0.9933333, f52=0.4066667, f404=0.26666668, f952=3.7000001,
         f956=1.7000001, step=0.016666668),
]

SPEC = """\
# Generated by tools/audit_live_flight_ramps_microexec.py --emit.
# {why}

function {start:#010x}
case live-ramps:{name}

region model {model:#010x} bytes:{model_bytes}
region vt    {vtable:#010x} bytes:{vtable_bytes}
region stack 0xC0000000 zero:0x4000

sp 0xC0002000
gpr r3 model
gpr r0 0
fpr f1 f:{step!r}
stub {slot38:#010x} slot 38, taken only on the bypass path and not modelled
dump model
"""


def vtable_bytes() -> str:
    blob = bytearray(4 * 40)
    struct.pack_into(">I", blob, 152, SLOT38_STUB)
    return blob.hex()


def model_bytes(entry) -> str:
    blob = bytearray(MODEL_SIZE)
    for field, value in entry["state"].items():
        struct.pack_into(">f", blob, field, f32(value))
    for field, value in entry["reads"].items():
        struct.pack_into(">f", blob, field, f32(value))
    struct.pack_into(">I", blob, FLAGS, entry["flags"])
    blob[GATE] = entry["gate"] & 0xFF
    struct.pack_into(">I", blob, 0, VTABLE)
    return blob.hex()


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for entry in CASES:
        spec = workdir / "specs" / f"{entry['name']}.spec"
        spec.write_text(SPEC.format(why=entry["why"], start=START,
                                    name=entry["name"], model=MODEL,
                                    model_bytes=model_bytes(entry),
                                    vtable=VTABLE, vtable_bytes=vtable_bytes(),
                                    slot38=SLOT38_STUB,
                                    step=f32(entry["step"])), encoding="utf-8")
        manifest.append(f"{spec.resolve()} "
                        f"{(workdir / 'out' / (entry['name'] + '.json')).resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(manifest)} manifest={workdir / 'manifest'}")
    return 0


def check(workdir: Path, report: Path | None) -> int:
    failures, compared, lines = [], 0, []
    for entry in CASES:
        path = workdir / "out" / f"{entry['name']}.json"
        if not path.exists():
            failures.append(f"{entry['name']}: no snapshot")
            continue
        document = json.loads(path.read_text(encoding="utf-8"))
        if document["exit"]["kind"] != "return":
            failures.append(f"{entry['name']}: exit {document['exit']}")
            continue
        if document["provenance"]["callee_entries"] != 0:
            failures.append(f"{entry['name']}: entered a callee; it calls nothing")
            continue
        # Slot 38 is dispatched only on the bypass path, so the stub count is
        # the evidence that bit 7 selects it and nothing else does.
        wanted = 1 if entry["flags"] & BYPASS else 0
        if len(document["calls"]) != wanted:
            failures.append(f"{entry['name']}: {len(document['calls'])} stubbed "
                            f"call(s), expected {wanted}")
            continue
        dump = next(e for e in document["region_dumps"] if e["name"] == "model")
        after = bytes.fromhex(dump["after_hex"])
        if after != bytes.fromhex(dump["after_hex_b"]):
            failures.append(f"{entry['name']}: the two poison passes disagree")
            continue
        want = expected(entry["state"], entry["reads"], entry["gate"],
                        entry["flags"], entry["step"])
        bad, got = [], {}
        for field in RAMPS:
            value = struct.unpack_from(">f", after, field)[0]
            got[field] = value
            compared += 1
            if struct.pack(">f", value) != struct.pack(">f", want[field]):
                bad.append(f"+{field}: retail {value!r} port {want[field]!r}")
        axes = {f: struct.unpack_from(">f", after, f)[0] for f in AXES}
        if bad:
            failures.append(f"{entry['name']}: " + "; ".join(bad))
        lines.append(f"{entry['name']}\t{'ok' if not bad else 'MISMATCH'}\t"
                     + "\t".join(repr(got[f]) for f in RAMPS) + "\t"
                     + "\t".join(repr(axes[f]) for f in AXES)
                     + f"\t{entry['why']}")
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "# 0x82303E68 (slot 30 of the LIVE flight model 0x8200F310)\n"
            "# micro-executed, against ac6::retail::update_live_flight_ramps.\n"
            "# The five ramp columns are COMPARED; the five axis columns are\n"
            "# recorded and NOT compared -- that block is not ported yet.\n"
            "# case\tverdict\t" + "\t".join(f"+{f}" for f in RAMPS) + "\t"
            + "\t".join(f"uncompared+{f}" for f in AXES)
            + "\twhy the case exists\n" + "\n".join(lines) + "\n",
            encoding="utf-8")
    for problem in failures:
        print("  FAIL  " + problem)
    print(f"live_flight_ramps_microexec={'pass' if not failures else 'fail'} "
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
