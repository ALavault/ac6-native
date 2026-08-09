#!/usr/bin/env python3
"""Map device button bits onto record flag bits, one bit at a time, by execution.

Cycle 1320 ran 0x821CAA50 with the held-button word set to 0x1000 and found bit 5
of the record flag word set. Bit 12 in, bit 5 out: the consumer does not copy the
button word, it remaps it. Nothing in the campaign had read that mapping, and
thirteen or/stw pairs at 0x821CABE0..0x821CAC4C are where it lives.

This measures it instead. One case per device bit, that bit alone set, plus a
null control with no bit set at all -- because a flag bit that appears in the
null case did not come from the buttons and would otherwise be attributed to
whichever bit happened to be under test.

WHY PASS B IS THE ANSWER HERE, AND PASS A IS NOT. The flag word is accumulated
with lwz/or/stw, so under the 0xCD poison it reads 0xCD|mask and every mask bit
0xCD already carries -- bits 0, 2, 3, 6 and 7 of each byte -- is invisible.
Cycle 1320's "bit 5 and no other" was a claim pass A could not support, and this
file will not repeat it: `after_hex_b` is the 0x00 pass, where the byte reads the
mask itself.

That pass is also the physically right one. The frame stage 0x821CA908 clears
+0x00..+0x83 of every record before the consumer runs, so a zero-filled record
region is the consumer's real precondition and 0xCD is the artificial one.

    python3 tools/audit_input_button_map_microexec.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_input_button_map_microexec.py --check --workdir W [--report R]
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

# The object model is the consumer tool's, not a second copy of it: a divergence
# between two builds of the same service would be indistinguishable from a
# finding.
from audit_input_consumer_microexec import (  # noqa: E402
    CONSUMER, CONTEXT, CONTROLLER_0, CONTROLLER_STRIDE, CONTEXT_VTABLE,
    CONTROLLER_VTABLE, HINTS, LOCK_STUBS, MEMSET, RECORD_0, RECORD_STRIDE,
    RECORDS, SERVICE,
)

# Device offsets of the four button words (cycle 1310's snapshot layout).
HELD = 0x1C
PRESSED = 0x14
RELEASED = 0x18
COMPLEMENT = 0x20

# Which device word each sweep drives. `held` first because it is the one cycle
# 1320 accidentally exercised; the other two are what a game reads for edges.
SWEEPS = {"held": HELD, "pressed": PRESSED, "released": RELEASED}

# THE OPEN QUESTION FROM CYCLE 1318: record flag bits 14 and 15 exist -- they own
# float slots +0x44 and +0x48 under the (bit + 3) * 4 rule -- and none of the
# eight axis halves feeds them. That cycle guessed the triggers and wrote the
# guess down as a guess.
#
# These are the candidates, each driven to its maximum on its own. Two are the
# halfwords at device+0x38/+0x3A that cycle 1318 found between the axis halves
# and the raw thumbs and could not place; two are the trigger bytes, which sit
# beyond device+0x43 and are therefore OUTSIDE the 0x40-byte snapshot copy -- so
# if they reach a record at all, the consumer read the device directly.
FIELD_SWEEPS = [
    # device+0x38 and +0x3A ARE the feed for record flag bits 14 and 15 -- they
    # fill the float slots at +0x44 and +0x48, which is (14+3)*4 and (15+3)*4.
    # Cycle 1318 found these two halfwords between the axis halves and the raw
    # thumbs, could not place them, and guessed the triggers instead.
    #
    # Three values each, because one value cannot distinguish a rule. 0xFFFF is
    # int16 -1 and gives -1/32767, which already rules out the split-halves rule
    # the eight axis halves take: that would read 0xFFFF as a magnitude and give
    # -2.0, or as a half-pair and give zero.
    ("dev38", 0x38, 2, 0xFFFF),
    ("dev3a", 0x3A, 2, 0xFFFF),
    ("dev38-pos", 0x38, 2, 20000),
    ("dev3a-pos", 0x3A, 2, 20000),
    ("dev38-min", 0x38, 2, 0x8000),
    ("dev3a-min", 0x3A, 2, 0x8000),
    ("dev38-small", 0x38, 2, 0x100),
    # THE FLAG BIT IS NOT THE SLOT. The slot at +0x44 is written for every value
    # above, negative ones included; the flag bit is set for +20000 and +256 and
    # NOT for -1 or -32768. So it is not the 0x800 deadzone cycle 1315 read on
    # the other path -- 0x100 is well under that and still sets it. These two
    # bracket the boundary: the smallest positive value, and a negative one big
    # enough that a magnitude test would pass.
    ("dev38-one", 0x38, 2, 1),
    ("dev38-neg", 0x38, 2, 0xFF00),
] + [
    # AND IT IS NOT A SIGN TEST EITHER. +1 does not set the flag bit and +256
    # does, so there is a threshold strictly between them. A bracket rather than
    # a guess: nine powers of two, one case each, in the same batch.
    (f"dev38-t{value:05d}", 0x38, 2, value)
    for value in (2, 4, 8, 16, 17, 18, 20, 24, 28, 29, 30, 31, 32, 64, 128, 192, 255)
] + [
    ("trigL", 0x4A, 1, 0xFF),
    ("trigR", 0x4B, 1, 0xFF),
    # THE RAW THUMBS. 0x8234D110 copies the four XINPUT_STATE thumbs verbatim to
    # device+0x3C..+0x42 alongside the split halves, so the consumer sees both
    # forms of the same stick. Nothing had asked what it does with the raw ones.
    #
    # There is a reason to ask. Cycle 1315 read a SECOND normalisation at
    # 0x821CB244 -- `subi r9,r9,0x4000` then a 0x800 threshold then a multiply by
    # float32(1/16383) -- which is not the one the axis stage takes: the executed
    # run put 30000 through float32(1/32767) and landed on 0.915555 exactly. The
    # biased path is later in the function and has never been reached.
    #
    # 0x7FFF - 0x4000 = 0x3FFF = 16383, so the biased path maps a full-scale raw
    # thumb onto exactly 1.0. That is a hypothesis, and these four cases are what
    # it takes to stop it being one.
    ("rawLX", 0x3C, 2, 0x7FFF),
    ("rawLY", 0x3E, 2, 0x7FFF),
    ("rawRX", 0x40, 2, 0x7FFF),
    ("rawRY", 0x42, 2, 0x7FFF),
    # A POSITIVE CONTROL FOR THE FLOAT READER BELOW. The LY positive half is
    # known to fill the slot at +0x50; if it does not show up here, the reader is
    # broken and every blank result above is worthless.
    ("halfLY", 0x28, 2, 30000),
]

# The float array: slot for flag bit b is (b + 3) * 4, so bits 0..31 would span
# +0x0C..+0x88. Only +0x0C..+0x5B has ever been seen written; the window read
# here is deliberately wider than that.
FLOAT_FIRST = 0x0C
FLOAT_LAST = 0x88


def build_service(word_offset: int | None, bit: int | None,
                  field: tuple[int, int, int] | None = None) -> bytes:
    """One controller in slot 0 with a single button bit set, and nothing else.

    Slot 0 is the only slot this path reads (cycle 1320, three plans), so one
    controller is enough and four would only add ways to be wrong.
    """
    span = (CONTROLLER_0 + CONTROLLER_STRIDE) - SERVICE + 0x40
    blob = bytearray(span)

    def put32(address: int, value: int) -> None:
        struct.pack_into(">I", blob, address - SERVICE, value & 0xFFFFFFFF)

    put32(SERVICE + 0x00, 1)
    put32(CONTEXT + 0x00, CONTEXT_VTABLE)
    for index in range(5):
        put32(CONTEXT + 0x04 + 4 * index, CONTROLLER_0)
    put32(CONTROLLER_0 + 0x00, CONTROLLER_VTABLE)
    put32(CONTROLLER_0 + 0x08, 0)          # connection state: valid
    if word_offset is not None and bit is not None:
        put32(CONTROLLER_0 + word_offset, 1 << bit)
    if field is not None:
        offset, width, value = field
        struct.pack_into(">H" if width == 2 else ">B",
                         blob, CONTROLLER_0 + offset - SERVICE, value)
    return bytes(blob)


SPEC = """\
# Generated by tools/audit_input_button_map_microexec.py --emit.
# One device button bit set, everything else zero.

function {consumer:#010x}
case retail-input-buttons:{name}

region service {service:#010x} bytes:{service_blob}
region records {records:#010x} poison:0x290
region stack   0xC0000000 zero:0x2000

sp 0xC0001800
gpr r3 0xC0000100
gpr r0 0

vmx on
alias on
capture gpr:r0

{stubs}
"""


def cases() -> list[dict]:
    built = [{"name": "null", "sweep": None, "bit": None,
              "service_blob": build_service(None, None).hex()}]
    for sweep, offset in SWEEPS.items():
        for bit in range(32):
            built.append({
                "name": f"{sweep}-bit{bit:02d}",
                "sweep": sweep,
                "bit": bit,
                "service_blob": build_service(offset, bit).hex(),
            })
    for name, offset, width, value in FIELD_SWEEPS:
        built.append({
            "name": f"field-{name}",
            "sweep": f"device+0x{offset:02X}",
            "bit": None,
            "service_blob": build_service(None, None,
                                          (offset, width, value)).hex(),
        })
    return built


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    stubs = "\n".join(f"stub {address:#010x} lock, not read"
                      for address in LOCK_STUBS)
    stubs += f"\nstub {MEMSET:#010x} vectorised memset; its targets are already zero"
    for hint in HINTS:
        stubs += f"\nhint {hint}"
    manifest = []
    for case in cases():
        spec = workdir / "specs" / f"{case['name']}.spec"
        spec.write_text(SPEC.format(consumer=CONSUMER, name=case["name"],
                                    service=SERVICE,
                                    service_blob=case["service_blob"],
                                    records=RECORDS, stubs=stubs),
                        encoding="utf-8")
        out = workdir / "out" / f"{case['name']}.json"
        manifest.append(f"{spec.resolve()} {out.resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(manifest)} manifest={workdir / 'manifest'}")
    return 0


def clean_memory(document: dict) -> dict[int, int] | None:
    """Record bytes as the 0x00 poison pass left them, keyed by address."""
    memory: dict[int, int] = {}
    for run in document.get("memory_writes", []):
        address = int(run["address"], 16)
        clean = run.get("after_hex_b")
        if clean is None:
            return None
        for index, value in enumerate(bytes.fromhex(clean)):
            memory[address + index] = value
    return memory


def flag_word(memory: dict[int, int]) -> int:
    base = RECORD_0 + 0x08
    return int.from_bytes(bytes(memory.get(base + n, 0) for n in range(4)), "big")


def float_slots(memory: dict[int, int]) -> dict[int, float]:
    """Every float slot the run WROTE, by record offset.

    Strict: a slot whose four bytes are not all present was not written, and is
    left out rather than read as 0.0. That is the opposite of the flag word's
    reader and for the opposite reason -- a mask byte of zero is a value, a
    float of +0.0 written is not distinguishable from one not written, so this
    reader refuses to guess.
    """
    found: dict[int, float] = {}
    for offset in range(FLOAT_FIRST, FLOAT_LAST, 4):
        address = RECORD_0 + offset
        if any(address + n not in memory for n in range(4)):
            continue
        raw = bytes(memory[address + n] for n in range(4))
        found[offset] = struct.unpack(">f", raw)[0]
    return found


def check(workdir: Path, report: Path | None) -> int:
    rows: list[dict] = []
    baseline: int | None = None
    baseline_slots: dict[int, float] = {}
    stale = []
    for case in cases():
        path = workdir / "out" / f"{case['name']}.json"
        if not path.exists():
            print(f"{case['name']}: no snapshot", file=sys.stderr)
            return 1
        document = json.loads(path.read_text(encoding="utf-8"))
        if document["exit"]["kind"] != "return":
            print(f"{case['name']}: exit={document['exit']}", file=sys.stderr)
            return 1
        memory = clean_memory(document)
        if memory is None:
            stale.append(case["name"])
            continue
        word = flag_word(memory)
        slots = float_slots(memory)
        if case["name"] == "null":
            baseline = word
            baseline_slots = slots
            print(f"null control: flag word 0x{word:08X}  "
                  f"float slots {{{', '.join(f'0x{o:02X}={v:g}' for o, v in slots.items())}}}")
            continue
        rows.append({"sweep": case["sweep"], "bit": case["bit"], "flags": word,
                     "name": case["name"], "slots": slots})

    if stale:
        print(f"{len(stale)} snapshots carry no after_hex_b; regenerate them with "
              "the current harness", file=sys.stderr)
        return 1
    if baseline is None:
        print("no null control", file=sys.stderr)
        return 1

    mapping: list[dict] = []
    for row in rows:
        # What THIS bit contributed, over and above what the null run produced.
        added = row["flags"] & ~baseline
        row["added"] = added
        if added:
            mapping.append({
                "device_word": row["sweep"],
                "device_bit": row["bit"],
                "case": row["name"],
                "record_flag_bits": [b for b in range(32) if added >> b & 1],
            })

    # A slot whose value differs from the null run's is this input's doing.
    slot_effects = []
    for row in rows:
        changed = {offset: value for offset, value in row["slots"].items()
                   if baseline_slots.get(offset) != value}
        if changed:
            slot_effects.append({"case": row["name"], "slots": {
                f"0x{o:02X}": round(v, 6) for o, v in sorted(changed.items())}})

    print(f"cases={len(rows)} baseline=0x{baseline:08X} mapped={len(mapping)} "
          f"slot_effects={len(slot_effects)}")
    for effect in slot_effects:
        print(f"  {effect['case']:16s} -> " +
              "  ".join(f"{o}={v}" for o, v in effect["slots"].items()))
    for entry in mapping:
        bits = ",".join(str(b) for b in entry["record_flag_bits"])
        which = ("bit --" if entry["device_bit"] is None
                 else f"bit {entry['device_bit']:2d}")
        print(f"  {entry['device_word']:13s} {which} "
              f"-> record flag bit(s) {bits}")

    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({
            "schema": "ac6.retail-input-button-map.v1",
            "statement": "one device button bit set per run of 0x821CAA50, with a "
                         "null control, read from the 0x00 poison pass because the "
                         "flag word is accumulated with or/stw and the 0xCD pass "
                         "hides every mask bit 0xCD already carries",
            "consumer": f"0x{CONSUMER:08X}",
            "record": f"0x{RECORD_0:08X}",
            "record_stride": f"0x{RECORD_STRIDE:02X}",
            "flag_word_offset": "0x08",
            "null_control_flags": f"0x{baseline:08X}",
            "device_words": {name: f"device+0x{offset:02X}"
                             for name, offset in SWEEPS.items()},
            "mapping": mapping,
            "float_slot_effects": slot_effects,
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
