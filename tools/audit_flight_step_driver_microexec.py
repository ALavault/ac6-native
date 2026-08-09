#!/usr/bin/env python3
"""Run the flight model's step and compare the whole composed result.

This is the first COMPOSITE differential of A3.2. Every previous one measured
one function against one port; this one runs 0x82283898 -- slot 11, the flight
model's per-frame entry point -- with the REAL slot 30 (0x82302DB0) reached
through a real virtual dispatch, and compares the object against
`ac6::retail::apply_flight_step`, which composes the contracted control-surface
port with the step's own reset.

So it tests three things no per-function audit can:

  - that the dispatch reaches slot 30 at all, through a vtable this audit builds
    rather than one it assumes;
  - that the step hands slot 30 ITS OWN float, unchanged;
  - that the reset runs AFTER slot 30 and clears five of its eight outputs,
    leaving +368, +372 and +376 alone.

WHAT IS STUBBED, and why that is not a hole. Slots 31 and 32 -- the position
integrator and the orientation update -- are contracted separately, take the
POSITION BLOCK rather than the model, and contain VMX128 that this instrument
cannot execute without the register-file bridge. They are stubbed at their real
addresses, which the synthetic vtable holds verbatim, so the dispatch being
exercised is the real one even where the callee is not.

HOW THE RESET BIT WAS FOUND. Not by reading `rlwinm r11,r11,31,31,31` -- by
running flags 0, 1, 2 and 3 and counting stubbed calls: bit 0 gave four calls
and no change, bit 1 gave five and zeroed exactly five fields. Cycles 1371 and
1375 had both written "bit 0". The measurement came first and the decode
confirmed it: a rotate left by 31 is a rotate right by one, which selects bit 1.

    python3 tools/audit_flight_step_driver_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_flight_step_driver_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

STEP = 0x82283898
MODEL = 0xB8000000
VTABLE = 0xB9000000
POSITION = 0xB4000000
MODEL_SIZE = 0x600

SLOT30 = 0x82302DB0        # run for real
STUBBED = ((31, 0x82303110, "the position integrator, contracted separately"),
           (32, 0x82302C88, "the orientation update, contracted separately"),
           (33, 0x82282C50, "slot 33, not read"))
PLAIN_STUBS = ((0x82282938, "not read"), (0x82326FE8, "not read"))

RESET_BIT = 1 << 1
STATE = (304, 308, 312, 360, 364, 368, 372, 376)
INPUTS = (36, 40, 44, 48, 52, 344)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_flight_controls_microexec import expected as slot30_expected  # noqa: E402


def f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


def composed(state, reads, flags, step):
    """apply_flight_step, restated: slot 30 then the conditional reset."""
    out = dict(slot30_expected(state, reads, flags, step))
    if flags & RESET_BIT:
        for field in (360, 364, 304, 308, 312):
            out[field] = 0.0
    return out


def case(name, why, flags=0, step=0.016666668, **kwargs):
    state = {field: 0.0 for field in STATE}
    reads = {field: 0.0 for field in INPUTS}
    for key, value in kwargs.items():
        field = int(key[1:])
        (state if field in STATE else reads)[field] = value
    return {"name": name, "why": why, "flags": flags, "step": step,
            "state": state, "reads": reads}


CASES = [
    case("idle", "nothing held, nothing set"),
    case("dispatch-reaches-slot-30",
         "a held ramp from zero: the object must move, which only happens if the "
         "virtual dispatch found 0x82302DB0", f48=1.0),
    case("float-is-threaded",
         "a step ten times larger must move the ramp ten times further",
         f48=1.0, step=0.16666668),
    case("no-reset-is-slot-30",
         "with the bit clear the step must equal slot 30 exactly",
         f304=0.5, f308=-0.5, f312=0.75, f360=0.5, f364=0.25, f368=0.5,
         f372=0.5, f48=1.0),
    case("reset-clears-five",
         "bit 1 clears five fields and spares +368, +372 and +376",
         flags=RESET_BIT, f304=0.5, f308=-0.5, f312=0.75, f360=0.5, f364=0.25,
         f368=0.5, f372=0.5, f48=1.0),
    case("bit-zero-does-nothing",
         "bit 0 is NOT the reset -- cycles 1371 and 1375 both said it was",
         flags=1, f304=0.5, f308=-0.5, f312=0.75, f360=0.5, f364=0.25,
         f368=0.5, f372=0.5, f48=1.0),
    case("reset-after-slot-30",
         "both ramps live so slot 30's interlock fires, then the reset: the "
         "survivors must be slot 30's values and not the seeds",
         flags=RESET_BIT, f360=1.0, f364=1.0, f368=0.5, f372=0.5, f48=1.0,
         f52=1.0),
    case("full-mantissas", "long binary expansions through the whole chain",
         flags=RESET_BIT | 4, f36=0.37777779, f40=-0.633333, f44=-0.1, f48=1.0,
         f52=1.0, f344=3.7000001, f304=0.13333334, f308=-0.26666668,
         f312=0.7333333, f360=0.9933333, f364=0.4066667, f368=0.6733333,
         f372=0.11333334, step=0.016666668),
]

SPEC = """\
# Generated by tools/audit_flight_step_driver_microexec.py --emit.
# {why}
#
# The vtable is synthetic but its entries are the REAL addresses, so the
# dispatch under test is the real one even where the callee is stubbed.

function {step_fn:#010x}
case flight-step-driver:{name}

region model {model:#010x} bytes:{model_bytes}
region vt    {vtable:#010x} bytes:{vtable_bytes}
region pos   {position:#010x} zero:0x100
region stack 0xC0000000 zero:0x2000

sp 0xC0001000
gpr r3 model
gpr r0 0
fpr f1 f:{step!r}
{stubs}dump model
"""


def vtable_bytes() -> str:
    blob = bytearray(4 * 36)
    struct.pack_into(">I", blob, 4 * 30, SLOT30)
    for index, address, _ in STUBBED:
        struct.pack_into(">I", blob, 4 * index, address)
    return blob.hex()


def model_bytes(entry) -> str:
    blob = bytearray(MODEL_SIZE)
    struct.pack_into(">I", blob, 0, VTABLE)
    struct.pack_into(">I", blob, 112, POSITION)
    struct.pack_into(">I", blob, 332, entry["flags"])
    for field, value in entry["state"].items():
        struct.pack_into(">f", blob, field, f32(value))
    for field, value in entry["reads"].items():
        struct.pack_into(">f", blob, field, f32(value))
    return blob.hex()


def stub_lines() -> str:
    lines = [f"stub {address:#010x} {note}" for _, address, note in STUBBED]
    lines += [f"stub {address:#010x} {note}" for address, note in PLAIN_STUBS]
    return "\n".join(lines) + "\n"


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for entry in CASES:
        spec = workdir / "specs" / f"{entry['name']}.spec"
        spec.write_text(SPEC.format(
            why=entry["why"], step_fn=STEP, name=entry["name"], model=MODEL,
            model_bytes=model_bytes(entry), vtable=VTABLE,
            vtable_bytes=vtable_bytes(), position=POSITION,
            step=f32(entry["step"]), stubs=stub_lines()), encoding="utf-8")
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
        # Four stubbed calls without the reset, five with it: the count IS the
        # evidence that slot 33 runs only on the reset path.
        wanted_calls = 5 if entry["flags"] & RESET_BIT else 4
        if len(document["calls"]) != wanted_calls:
            failures.append(f"{entry['name']}: {len(document['calls'])} stubbed "
                            f"call(s), expected {wanted_calls}")
            continue
        dump = next(e for e in document["region_dumps"] if e["name"] == "model")
        after = bytes.fromhex(dump["after_hex"])
        if after != bytes.fromhex(dump["after_hex_b"]):
            failures.append(f"{entry['name']}: the two poison passes disagree")
            continue
        want = composed(entry["state"], entry["reads"], entry["flags"],
                        entry["step"])
        bad, got = [], {}
        for field in STATE:
            value = struct.unpack_from(">f", after, field)[0]
            got[field] = value
            compared += 1
            if struct.pack(">f", value) != struct.pack(">f", want[field]):
                bad.append(f"+{field}: retail {value!r} port {want[field]!r}")
        if bad:
            failures.append(f"{entry['name']}: " + "; ".join(bad))
        lines.append(f"{entry['name']}\t{'ok' if not bad else 'MISMATCH'}\t"
                     + "\t".join(repr(got[f]) for f in STATE)
                     + f"\t{entry['why']}")
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            "# 0x82283898 (slot 11, the step) micro-executed with the REAL slot\n"
            "# 30 reached through a real virtual dispatch, against\n"
            "# ac6::retail::apply_flight_step.\n"
            "# case\tverdict\t" + "\t".join(f"+{f}" for f in STATE)
            + "\twhy the case exists\n" + "\n".join(lines) + "\n",
            encoding="utf-8")
    for problem in failures:
        print("  FAIL  " + problem)
    print(f"flight_step_driver_microexec={'pass' if not failures else 'fail'} "
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
