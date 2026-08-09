#!/usr/bin/env python3
"""Run the retail flight control update and compare it against the port.

`include/ac6/retail_flight_controls.h` derives 0x82302DB0 -- slot 30 of the
vtable 0x8200F270, the first of the three pure virtuals the step 0x82283898
calls. This runs the retail instructions and compares all eight words it writes.

WHY THIS ONE IS EASY AND THE INTEGRATOR WAS NOT. 0x82302DB0 has **no call and no
vector instruction** in 216 instructions, and its whole state is one object
reached through r3. So it executes end to end from its own entry, returns
normally, and needs no mid-function seeding, no epsilon trick and no step cap.
That was measured before the function was read: a probe seeded the model with a
per-offset pattern and dumped it back, and the eight words that changed are
exactly the eight `stfs` offsets in the listing.

    python3 tools/audit_flight_controls_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_flight_controls_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

START = 0x82302DB0
MODEL = 0xB8000000
MODEL_SIZE = 0x180          # +376 is the highest offset touched

READS = (36, 40, 44, 48, 52, 344)
STATE = (304, 308, 312, 360, 364, 368, 372, 376)
FLAGS = 332

C = {
    "ramp_rate": 3.3333332538604736,      # 0x82007B90, 10/3
    "ramp_decay": 0.5,                    # 0x82001354
    "second_rate": 10.0,                  # 0x82003214
    "second_decay": 0.800000011920929,    # 0x82069ECC
    "gate": 0.9900000095367432,           # 0x82069E50
    "axis_rate": 1.6666666269302368,      # 0x82007B8C, 5/3
    "at308_rate": 2.5,                    # 0x82002FDC
    "centring": 0.699999988079071,        # 0x82002FD0
    "neg_gain": 0.8999999761581421,       # 0x82069C3C
    "lower": -0.8999999761581421,         # 0x82007F84
    "offset": 0.10000000149011612,        # 0x82002FD4
}


def f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


def fmaf(a: float, b: float, c: float) -> float:
    return f32(math.fma(a, b, c))


def expected(state: dict, reads: dict, flags: int, step: float) -> dict:
    """The port's rule, restated from the listing so the two sides are
    independent code. Every input is rounded to single first: cycle 1373 lost a
    run to an oracle computing from doubles while the emulator was seeded with
    singles."""
    s = {k: f32(v) for k, v in state.items()}
    r = {k: f32(v) for k, v in reads.items()}
    step = f32(step)

    rise = f32(step * C["ramp_rate"])
    decay = f32(rise * C["ramp_decay"])
    for field, hold in ((360, 48), (364, 52)):
        value = f32(s[field] - decay)
        if value < 0.0:
            value = 0.0
        if r[hold] != 0.0:
            value = f32(value + rise)
            if value > 1.0:
                value = 1.0
        s[field] = value

    rise2 = f32(step * C["second_rate"])
    decay2 = f32(rise2 * C["second_decay"])
    for field, primary in ((368, 360), (372, 364)):
        value = f32(s[field] - decay2)
        if value < 0.0:
            value = 0.0
        if s[primary] > C["gate"]:
            value = f32(value + rise2)
            if value > 1.0:
                value = 1.0
        else:
            value = 0.0
        s[field] = value
    if s[360] != 0.0 and s[364] != 0.0:
        s[368] = 0.0
        s[372] = 0.0
    s[376] = f32(s[360] - s[364])

    rate = f32(step * C["axis_rate"])
    rate308 = f32(step * C["at308_rate"])
    rate312 = rate
    if (flags >> 2) & 1:
        scale = f32(1.0 / f32(f32(r[344] + C["offset"]) * C["second_rate"]))
        rate = f32(rate * scale)
        rate308 = f32(rate308 * scale)
        rate312 = f32(rate312 * scale)

    def centre(value: float, this_rate: float) -> float:
        if value > 0.0:
            value = fmaf(-this_rate, C["centring"], value)
            if value < 0.0:
                value = 0.0
        elif value < 0.0:
            value = fmaf(this_rate, C["centring"], value)
            if value > 0.0:
                value = 0.0
        return value

    s[304] = centre(s[304], rate)
    if r[36] != 0.0:
        gain = 1.0 if r[36] > 0.0 else C["neg_gain"]
        s[304] = fmaf(f32(r[36] * gain), rate, s[304])
        if s[304] > 1.0:
            s[304] = 1.0
        elif s[304] < C["lower"]:
            s[304] = C["lower"]

    s[312] = centre(s[312], rate312)
    if r[40] != 0.0:
        s[312] = fmaf(r[40], rate312, s[312])
        if s[312] > 1.0:
            s[312] = 1.0
        elif s[312] < -1.0:
            s[312] = -1.0

    s[308] = centre(s[308], rate308)
    if r[44] > 0.0:
        s[308] = f32(s[308] + rate308)
        if s[308] > 1.0:
            s[308] = 1.0
    elif r[44] < 0.0:
        s[308] = f32(s[308] - rate308)
        if s[308] < -1.0:
            s[308] = -1.0
    return s


def case(name, why, step=0.0166666666, flags=0, **kwargs):
    state = {field: 0.0 for field in STATE}
    reads = {field: 0.0 for field in READS}
    for key, value in kwargs.items():
        field = int(key[1:])
        (state if field in STATE else reads)[field] = value
    return {"name": name, "why": why, "step": step, "flags": flags,
            "state": state, "reads": reads}


CASES = [
    case("idle", "nothing held, nothing commanded, everything already zero"),
    case("ramp-rise", "hold48 raises at360 from zero", f48=1.0),
    case("ramp-decay", "at360 falls at half the rise rate when not held", f360=0.5),
    case("ramp-floor", "the decay stops at zero and does not go negative",
         f360=0.0001),
    case("ramp-ceiling", "the rise saturates at 1.0", f360=0.99, f48=1.0),
    case("gate-open", "at360 past 0.99 lets at368 rise", f360=1.0, f48=1.0),
    case("gate-shut", "at360 below the gate ASSIGNS zero rather than decaying",
         f360=0.5, f368=0.5),
    case("interlock", "BOTH primaries non-zero kills both secondaries",
         f360=1.0, f364=1.0, f48=1.0, f52=1.0, f368=0.5, f372=0.5),
    case("interlock-one-zero", "one primary at zero leaves the secondaries alone",
         f360=1.0, f48=1.0, f368=0.5),
    case("difference", "at376 is at360 - at364 after both are updated",
         f360=0.75, f364=0.25),
    case("centre-positive", "a positive axis is pulled toward zero", f304=0.5,
         f308=0.5, f312=0.5),
    case("centre-negative", "a negative axis is pulled toward zero", f304=-0.5,
         f308=-0.5, f312=-0.5),
    case("centre-no-overshoot", "centring stops at zero, it does not cross",
         f304=0.0001, f308=0.0001, f312=0.0001),
    case("cmd304-positive", "gain 1.0 above zero", f36=0.8),
    case("cmd304-negative", "gain 0.9 below zero -- asymmetric", f36=-0.8),
    case("cmd304-clamps", "the limits are +1.0 and -0.9, not symmetric",
         f36=-1.0, f304=-0.89, step=1.0),
    case("cmd312", "at312 scales its command by the rate", f40=0.7, f312=0.2),
    case("cmd308-sign-only", "at308 uses the SIGN of cmd44, not its magnitude",
         f44=0.01, f308=0.2),
    case("cmd308-sign-only-large", "the same output for a much larger command",
         f44=900.0, f308=0.2),
    case("rate-scale-off", "bit 2 clear leaves the rates alone",
         f36=0.5, f40=0.5, f44=1.0, f344=4.0, flags=0),
    case("rate-scale-on", "bit 2 set divides all three by (f344+0.1)*10",
         f36=0.5, f40=0.5, f44=1.0, f344=4.0, flags=4),
    case("rate-scale-other-bits", "only bit 2 matters, not the whole word",
         f36=0.5, f40=0.5, f44=1.0, f344=4.0, flags=0xFFFB),
    case("full-mantissas", "long binary expansions everywhere -- the fused "
         "operations only differ here",
         f36=0.37777779, f40=-0.633333, f44=-0.1, f48=1.0, f52=1.0,
         f344=3.7000001, f304=0.13333334, f308=-0.26666668, f312=0.7333333,
         f360=0.9933333, f364=0.4066667, f368=0.6733333, f372=0.11333334,
         flags=4, step=0.016666668),
]

SPEC = """\
# Generated by tools/audit_flight_controls_microexec.py --emit.
# {why}

function {start:#010x}
case flight-controls:{name}

region model {model:#010x} bytes:{model_bytes}
region stack 0xC0000000 zero:0x2000

sp 0xC0001000
gpr r3 model
gpr r0 0
fpr f1 f:{step!r}
dump model
"""


def model_bytes(entry) -> str:
    blob = bytearray(MODEL_SIZE)
    for field, value in entry["state"].items():
        struct.pack_into(">f", blob, field, f32(value))
    for field, value in entry["reads"].items():
        struct.pack_into(">f", blob, field, f32(value))
    struct.pack_into(">I", blob, FLAGS, entry["flags"])
    return blob.hex()


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for entry in CASES:
        spec = workdir / "specs" / f"{entry['name']}.spec"
        spec.write_text(SPEC.format(why=entry["why"], start=START, name=entry["name"],
                                    model=MODEL, model_bytes=model_bytes(entry),
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
            failures.append(f"{entry['name']}: entered a callee; this function calls nothing")
            continue
        dump = next(e for e in document["region_dumps"] if e["name"] == "model")
        after_a = bytes.fromhex(dump["after_hex"])
        if after_a != bytes.fromhex(dump["after_hex_b"]):
            failures.append(f"{entry['name']}: the two poison passes disagree")
            continue
        want = expected(entry["state"], entry["reads"], entry["flags"], entry["step"])
        bad = []
        got = {}
        for field in STATE:
            value = struct.unpack_from(">f", after_a, field)[0]
            got[field] = value
            compared += 1
            if struct.pack(">f", value) != struct.pack(">f", want[field]):
                bad.append(f"+{field}: retail {value!r} port {want[field]!r}")
        if bad:
            failures.append(f"{entry['name']}: " + "; ".join(bad))
        lines.append(f"{entry['name']}\t{'ok' if not bad else 'MISMATCH'}\t"
                     + "\t".join(repr(got[f]) for f in STATE) + f"\t{entry['why']}")
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "# 0x82302DB0 (slot 30 of 0x8200F270) micro-executed, against\n"
            "# ac6::retail::update_flight_controls.\n"
            "# case\tverdict\t"
            + "\t".join(f"+{f}" for f in STATE) + "\twhy the case exists\n"
            + "\n".join(lines) + "\n", encoding="utf-8")
    for problem in failures:
        print("  FAIL  " + problem)
    print(f"flight_controls_microexec={'pass' if not failures else 'fail'} "
          f"cases={len(CASES)} passed={len(CASES) - len(failures)} "
          f"values_compared={compared}")
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
