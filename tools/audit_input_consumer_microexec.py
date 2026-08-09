#!/usr/bin/env python3
"""Run 0x821CAA50 on a built input service and read what it writes.

Cycles 1315-1318 derived the consumer by reading: a flag word at record+0x08, a
parallel float array whose slot for bit b is (b + 3) * 4, four signed axis floats
at +0x4C..+0x58, a deadzone and two reciprocal scales. Every step is an
instruction, and none of it has been executed.

This executes it. The function reaches its snapshot through the service singleton
at 0x8290DE00, so the run builds that singleton, its DriverContext at +0x24, the
five driver pointers at context+0x04 and four DriverControllers -- as memory,
not as stubs, so the API path under test is the real one. Only the lock is
stubbed, because 0x823D6A7C/8C have not been read and a critical section is not
what this measures.

THE CONTROLLERS ARE MADE DISTINGUISHABLE, three ways, so the output says by
itself which one reached which record.

Cycle 1319's version of this docstring said "the index the API receives is a
constant 1 while the loop runs 0..3". THAT MISREAD WHICH ARGUMENT WAS WHICH.
0x821CAA9C calls 0x82337E70 with (1, r20): the constant 1 is the first argument,
and the index is r20, which cycle 1318 found written once, `li r20,0x0`, before
any read. Cycle 1320 then checked all three callers -- 0x821CA908, 0x821CB5F0
and the frame update 0x821D7A90 -- and none of them writes or even reads r20, so
the caller cannot supply one either.

The run agrees: all four records carry driver-pointer slot 0.

    python3 tools/audit_input_consumer_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_input_consumer_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

CONSUMER = 0x821CAA50
SERVICE = 0x8290DE00          # cycle 1309
CONTEXT = SERVICE + 0x24
CONTROLLER_0 = CONTEXT + 0x18  # cycle 1308: four, stride 0x88
CONTROLLER_STRIDE = 0x88
CONTEXT_VTABLE = 0x820110C8
CONTROLLER_VTABLE = 0x820124F8
RECORDS = 0x826EDB98           # cycle 1313
# CYCLE 1316 PUT RECORD 0 AT RECORDS + 0x08 AND THAT IS WRONG BY EIGHT.
#
# The write runs begin at 0x826EDB9C, 0x826EDC3C, 0x826EDCDC and 0x826EDD7C --
# stride 0xA0, and four bytes above a base of 0x826EDB98, because +0x00..+0x03 is
# not written on this path. So the run addresses alone do NOT fix the base; they
# are consistent with 0x826EDB98 and with 0x826EDB9C both.
#
# What fixes it is a second derivation that never met the first: cycle 1318 read
# the LY axis into the float slot at +0x50 from the mask bits alone, and only a
# base of 0x826EDB98 puts float32(30000/32767) -- the value slot 0's controller
# carried -- at that offset. 0x826EDB9C would put it at +0x4C, which is LX.
RECORD_0 = RECORDS
RECORD_STRIDE = 0xA0
LOCK_STUBS = (0x823D6A7C, 0x823D6A8C, 0x823D6A9C)
# 0x821F3BB0 is memset, and it is VECTORISED -- it faults on vspltisb, which this
# Ghidra language leaves without emulation semantics. Stubbing it asserts nothing:
# its two calls zero r1+0x60 and r1+0x140, both inside a region the harness has
# already filled with zeros, so the stub and the call leave identical memory.
MEMSET = 0x821F3BB0
# Pure cache hints: the architecture defines them as having no effect on program
# state, so a no-op reproduces them rather than modelling them.
HINTS = ("dataCacheBlockTouch", "dataCacheBlockTouchForStore")

# Device offsets of the eight axis halves, from the table at 0x8201250C.
HALVES = {"LX+": 0x2E, "LX-": 0x2C, "LY+": 0x28, "LY-": 0x2A,
          "RX+": 0x36, "RX-": 0x34, "RY+": 0x30, "RY-": 0x32}
# Slot each axis lands in, and its sign, from cycle 1318.
SLOTS = {"LX": 0x4C, "LY": 0x50, "RX": 0x54, "RY": 0x58}
SCALE_HALF = 1.0 / 32767.0     # float32(1/32767) at 0x82069BFC

# THREE PLANS, BECAUSE ONE CANNOT ANSWER THE QUESTION.
#
# `distinct-axes` gives each controller a different axis, so a record names the
# controller it came from by WHICH slot is filled. `same-axis` gives all four the
# same axis at different magnitudes, so a record names it by WHAT VALUE. Run
# alone, the first is ambiguous: four identical records could mean every record
# reads controller 0, or that the axis simply never varies per record.
PLANS = {
    "distinct-axes": [
        {"axis": "LY", "half": "LY+", "value": 30000, "sign": +1},
        {"axis": "LX", "half": "LX-", "value": 25000, "sign": -1},
        {"axis": "RX", "half": "RX+", "value": 20000, "sign": +1},
        {"axis": "RY", "half": "RY-", "value": 15000, "sign": -1},
    ],
    "same-axis": [
        {"axis": "LY", "half": "LY+", "value": 30000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 25000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 20000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 15000, "sign": +1},
    ],
    # FIVE controllers, one per driver-pointer slot. The two plans above give
    # slots 0..3 distinct controllers and slot 4 a duplicate of 3, so "the value
    # came from controller 0" and "the value came from pointer slot 0" are the
    # same statement in both. This separates them: every slot has its own
    # controller and its own magnitude, so whichever slot is read names itself.
    "five-slots": [
        {"axis": "LY", "half": "LY+", "value": 30000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 25000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 20000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 15000, "sign": +1},
        {"axis": "LY", "half": "LY+", "value": 10000, "sign": +1},
    ],
}


def build_service(plan: list[dict]) -> tuple[int, bytes]:
    """The singleton, its context, five driver pointers and four controllers."""
    span = (CONTROLLER_0 + len(plan) * CONTROLLER_STRIDE) - SERVICE + 0x40
    blob = bytearray(span)

    def put32(address: int, value: int) -> None:
        struct.pack_into(">I", blob, address - SERVICE, value & 0xFFFFFFFF)

    def put16(address: int, value: int) -> None:
        struct.pack_into(">H", blob, address - SERVICE, value & 0xFFFF)

    put32(SERVICE + 0x00, 1)              # 0x82337E88 sets this after construction
    put32(CONTEXT + 0x00, CONTEXT_VTABLE)
    for index in range(5):
        # context+0x04 is five driver pointers, bound-checked at 5 (0x82343A30).
        # A plan shorter than five repeats its last controller, so every slot
        # points at a built object rather than at zero.
        which = min(index, len(plan) - 1)
        put32(CONTEXT + 0x04 + 4 * index, CONTROLLER_0 + which * CONTROLLER_STRIDE)

    for index, entry in enumerate(plan):
        base = CONTROLLER_0 + index * CONTROLLER_STRIDE
        put32(base + 0x00, CONTROLLER_VTABLE)
        put32(base + 0x08, 0)             # connection state: valid
        put32(base + 0x1C, 0x1000 + index)  # current buttons, distinguishable
        put16(base + HALVES[entry["half"]], entry["value"])
    return SERVICE, bytes(blob)


SPEC = """\
# Generated by tools/audit_input_consumer_microexec.py --emit.
# 0x821CAA50 on a built NU::Input service, one spec per controller plan, so the
# output names which driver-pointer slot reached which record.

function {consumer:#010x}
case retail-input-consumer:{name}

region service {service:#010x} bytes:{service_blob}
region records {records:#010x} poison:0x290
region stack   0xC0000000 zero:0x2000

sp 0xC0001800
gpr r3 {this:#010x}
gpr r0 0

# The accessor tail-calls a VECTORISED memcpy (0x821F398C), so the run needs the
# asserted semantics for lvsl and the register-file bridge -- the loop crosses
# the two files twice per iteration: lvx128 writes vr13, vperm reads vs45, vperm
# writes vs45, stvx128 reads vr13.
vmx on
alias on

# r0 IS AN INPUT TO THAT LOOP AND THE MODULE GETS IT WRONG. `lvx128 vr13,r0,r31`
# and `stvx128 vr13,r0,r30` emit INT_ADD(r0, rB) with no (rA|0) rule (cycle 1296,
# pinned as lvx128-ra-is-r0), so a nonzero r0 silently displaces every vector
# load and store in the copy. It is seeded zero above and captured below, so the
# check can say whether it stayed that way rather than assuming it.
capture gpr:r0

{stubs}
"""


def cases() -> list[dict]:
    built = []
    for name, plan in PLANS.items():
        base, blob = build_service(plan)
        built.append({
            "name": name,
            "plan": plan,
            "service": base,
            "service_blob": blob.hex(),
            # `this` is the caller's object; 0x821CA908 passes its own r3
            # through. It is read at +0x08 and +0x19/+0x1C, so it gets a slice
            # of the stack.
            "this": 0xC0000100,
        })
    return built


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for case in cases():
        stubs = "\n".join(f"stub {address:#010x} lock, not read"
                          for address in LOCK_STUBS)
        stubs += (f"\nstub {MEMSET:#010x} vectorised memset; its targets are "
                  "already zero in this run, so the stub is equivalent")
        for hint in HINTS:
            stubs += f"\nhint {hint}"
        spec = workdir / "specs" / f"{case['name']}.spec"
        spec.write_text(SPEC.format(consumer=CONSUMER, name=case["name"],
                                    service=case["service"],
                                    service_blob=case["service_blob"],
                                    records=RECORDS, this=case["this"],
                                    stubs=stubs), encoding="utf-8")
        out = workdir / "out" / f"{case['name']}.json"
        manifest.append(f"{spec.resolve()} {out.resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(manifest)} manifest={workdir / 'manifest'}")
    return 0


def check(workdir: Path, report: Path | None) -> int:
    findings: list[dict] = []
    for case in cases():
        path = workdir / "out" / f"{case['name']}.json"
        if not path.exists():
            print(f"{case['name']}: no snapshot", file=sys.stderr)
            return 1
        document = json.loads(path.read_text(encoding="utf-8"))
        # THE 0x00 PASS, NOT THE 0xCD PASS. Cycle 1320 read `after_hex` here
        # and reported record+0x0B as "bit 5 set and no other" -- a claim that
        # pass cannot support, because the flag word is accumulated with
        # lwz/or/stw and every mask bit 0xCD already carries (bits 0, 2, 3, 6, 7
        # of each byte) is hidden under the poison. `after_hex_b` is the run with
        # the record region zeroed, which is also the frame stage's own
        # precondition: 0x821CA908 clears +0x00..+0x83 before this runs.
        memory: dict[int, int] = {}
        for run in document.get("memory_writes", []):
            address = int(run["address"], 16)
            clean = run.get("after_hex_b")
            if clean is None:
                print(f"{case['name']}: snapshot predates after_hex_b; regenerate",
                      file=sys.stderr)
                return 1
            for index, value in enumerate(bytes.fromhex(clean)):
                memory[address + index] = value

        def word(address: int) -> int | None:
            if any(address + n not in memory for n in range(4)):
                return None
            return int.from_bytes(bytes(memory[address + n] for n in range(4)), "big")

        def mask_word(address: int) -> int:
            """A bitmask word, with undetected bytes read as zero.

            Legitimate here and nowhere else in this file. A byte is undetected
            exactly when it equalled the poison in BOTH passes, so in the 0x00
            pass it is zero -- for a mask that is a value, not an absence. The
            float slots keep the strict reader above, where a missing byte would
            otherwise read as 0.0 and look like a written zero.
            """
            return int.from_bytes(
                bytes(memory.get(address + n, 0) for n in range(4)), "big")

        print(f"{case['name']}: exit={document['exit']['kind']} "
              f"steps={document['provenance']['steps']} "
              f"r0_at_exit={document['registers'].get('r0')}")
        # The (rA|0) defect displaces every vector load and store in the copy if
        # r0 ever left zero. Cheap to check, fatal to ignore.
        if document["registers"].get("r0") != "0x00000000":
            print(f"{case['name']}: r0 is not zero at exit; the vectorised copy's "
                  "addresses cannot be trusted", file=sys.stderr)
            return 1
        for index in range(4):
            base = RECORD_0 + index * RECORD_STRIDE
            row = {
                "case": case["name"],
                "record": index,
                "base": f"0x{base:08X}",
                "flag_word": f"0x{mask_word(base + 0x08):08X}",
            }
            for axis, slot in SLOTS.items():
                raw = word(base + slot)
                row[axis] = (None if raw is None
                             else round(struct.unpack(">f", raw.to_bytes(4, "big"))[0], 6))
            findings.append(row)
            print(f"  record {index} @ 0x{base:08X}  "
                  f"flags={row['flag_word']}  " +
                  "  ".join(f"{axis}={row[axis]}" for axis in SLOTS))

    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({
            "schema": "ac6.retail-input-consumer-microexec.v1",
            "statement": "0x821CAA50 executed to return on a built NU::Input "
                         "service under three controller plans -- one axis per "
                         "controller, one value per controller, and one "
                         "controller per driver-pointer slot -- so a record "
                         "names its source by slot, by magnitude and by axis",
            "finding": "all four records are filled from driver-pointer slot 0, "
                       "under all three plans, at an identical 4740 steps: the "
                       "other four slots are not read on this path. r20 is the "
                       "index and it is zero (li r20,0x0 at 0x821CAA88, and none "
                       "of the three callers writes or reads it).",
            "record_base": f"0x{RECORD_0:08X}",
            "record_stride": f"0x{RECORD_STRIDE:02X}",
            "base_correction": "cycle 1316 put record 0 at 0x826EDBA0; the run "
                               "writes at 0x826EDB98 and only that base puts "
                               "float32(30000/32767) in the +0x50 slot cycle "
                               "1318 derived for LY",
            "consumer": f"0x{CONSUMER:08X}",
            "stubbed": [f"0x{address:08X}" for address in LOCK_STUBS]
                       + [f"0x{MEMSET:08X}"],
            "hint_noops": list(HINTS),
            "plans": PLANS,
            "records": findings,
        }, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {report}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--workdir", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--emit", action="store_true")
    mode.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    return emit(arguments.workdir) if arguments.emit else check(arguments.workdir,
                                                               arguments.report)


if __name__ == "__main__":
    raise SystemExit(main())
