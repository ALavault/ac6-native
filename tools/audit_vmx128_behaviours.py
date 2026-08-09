#!/usr/bin/env python3
"""Check each asserted VMX128 behaviour on its own, against a hand-computed answer.

Cycle 1295 implemented four operations the Xenon SLEIGH module leaves without
emulation semantics, validated none of them individually, and debugged them
through a 547-step composite of 271 instructions. The composite localised
nothing: three different inputs produced byte-identical output and there was no
way to say which of the four was at fault.

This is the missing half. Each case executes **one retail instruction**, with
every input seeded and the output register captured, and compares it against a
value worked out by hand and written down here beside it. A behaviour that
fails here fails alone.

The instruction addresses are real sites in the image, not synthesised
encodings, so the operand wiring under test is the wiring the harness will meet.

    python3 tools/audit_vmx128_behaviours.py --emit --workdir W
    <analyzeHeadless ... -postScript MicroExecuteFunction.java --batch W/manifest>
    python3 tools/audit_vmx128_behaviours.py --check --workdir W
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# ENDIANNESS ANCHOR. Xenon is big-endian, the host is not, and every lane-order
# verdict in this file depends on one convention: that the hex string a capture
# prints, read left to right, is increasing memory address -- so its first word
# is ISA element 0 (VD.x), the most significant.
#
# That is not assumed here, it is measured, by two independent controls:
#
#   * `lvlx+0` loads 16 bytes straight out of the fixture below. Its expectation
#     is written in MEMORY order. If the capture were mirrored, the test would
#     fail. It is the anchor, and check() refuses to report any lane-order
#     defect unless it passed.
#   * `vmrghw` and `vmrglw` are mirror images of each other with the operands
#     swapped: under a mirrored convention, vmrghw(A,B) reads as vmrglw(B,A).
#     Both pass here with distinct expectations and distinct XO-confirmed
#     encodings, which a mirrored convention cannot do.
#
# A 32-byte fixture with every byte distinct, so a misplaced lane is visible as
# a value rather than as a coincidence.
FIXTURE = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
FIXTURE_BASE = 0xB6000000
# A separate, poison-filled region for the one case that tests a store.
TARGET_BASE = 0xB7000000

# Two vectors whose words are all distinct and not palindromic, for the same
# reason. VA reads forward, VB backward.
VA = "00112233445566778899aabbccddeeff"
VB = "0f0e0d0c0b0a09080706050403020100"


def words(hex_text: str) -> list[str]:
    return [hex_text[i:i + 8] for i in range(0, 32, 8)]


VA_W, VB_W = words(VA), words(VB)

# vmrghw vD,vA,vB : vD = {vA[0], vB[0], vA[1], vB[1]}
EXPECT_MRGHW = VA_W[0] + VB_W[0] + VA_W[1] + VB_W[1]
# vmrglw vD,vA,vB : vD = {vA[2], vB[2], vA[3], vB[3]}
EXPECT_MRGLW = VA_W[2] + VB_W[2] + VA_W[3] + VB_W[3]

# vrlimi128 vD,vB,0x4,0x3 : rotate vB left by 3 words, then element i comes from
# the rotated vB when bit (3-i) of the mask is set and from vD otherwise.
# mask 0x4 = 0b0100 -> only element 1 comes from the rotated vB.
_ROT3 = [VB_W[(i + 3) & 3] for i in range(4)]
EXPECT_VRLIMI = VA_W[0] + _ROT3[1] + VA_W[2] + VA_W[3]

# lvlx: the bytes from EA to the end of EA's 16-byte block, left-justified,
# zero-filled. eb = EA & 0xF.
def expect_lvlx(offset: int) -> str:
    eb = offset & 0xF
    taken = FIXTURE[2 * offset: 2 * (offset + 16 - eb)]
    return taken + "00" * eb


CASES = [
    {
        "name": "vmrghw",
        # 0x820998E8  vmrghw v6,v10,v8 -> vs38 = CALLOTHER(vs42, vs40)
        "function": 0x820998E8,
        "vec": {"vs42": VA, "vs40": VB},
        "capture": "vs38",
        "expect": EXPECT_MRGHW,
    },
    {
        "name": "vmrglw",
        # 0x820998FC  vmrglw v10,v6,v5 -> vs42 = CALLOTHER(vs38, vs37)
        "function": 0x820998FC,
        "vec": {"vs38": VA, "vs37": VB},
        "capture": "vs42",
        "expect": EXPECT_MRGLW,
    },
    {
        "name": "vrlimi128",
        # 0x820A9B98  vrlimi128 vr12,vr13,0x4,0x3 -> vr12 = CALLOTHER(vr12, vr13, 4, 3)
        # The destination is an input: the instruction inserts under a mask.
        "function": 0x820A9B98,
        "vec": {"vr12": VA, "vr13": VB},
        "capture": "vr12",
        "expect": EXPECT_VRLIMI,
    },
]

# Instructions the SLEIGH module implements ITSELF, with no CALLOTHER to hook.
# They are tested the same way and for a stronger reason: a CALLOTHER the module
# declined to implement fails loudly, and a p-code body the module got wrong
# fails silently. `module: True` means no behaviour of ours should fire.
#
# vmsum4fp128 vrD,vrA,vrB : the 4-lane dot product, broadcast to all four lanes.
#   [1,2,3,4] . [10,20,30,40] = 10 + 40 + 90 + 160 = 300.0 = 0x43960000
MODULE_CASES = [
    {
        "name": "vmsum4fp128",
        # 0x820A9BE8  vmsum4fp128 vr4,vr0,vr11
        "function": 0x820A9BE8,
        "vec": {
            "vr0": "3f800000" "40000000" "40400000" "40800000",   # 1 2 3 4
            "vr11": "41200000" "41a00000" "41f00000" "42200000",  # 10 20 30 40
        },
        "capture": "vr4",
        "expect": "43960000" * 4,
        "module": True,
    },
    {
        # 0x820A9BEC  vpermwi128 vr13,vr12,0xac
        #
        # The VMX128 reference gives the encoding but not the lane order; Xenia's
        # InstrEmit_vpermwi128 gives it: VD.x = VB[uimm bits 6-7], VD.y = bits
        # 4-5, VD.z = bits 2-3, VD.w = bits 0-1. So the HIGH bit-pair selects the
        # FIRST word. uimm = 0xAC = 0b10101100 -> selectors 2, 2, 3, 0.
        "name": "vpermwi128",
        "function": 0x820A9BEC,
        "vec": {"vr12": VB},
        "capture": "vr13",
        "expect": VB_W[2] + VB_W[2] + VB_W[3] + VB_W[0],
        "module": True,
        # MEASURED, cycle 1297: the module returns the lane order REVERSED --
        # {vB[0], vB[3], vB[2], vB[2]} where the spec says {vB[2], vB[2], vB[3],
        # vB[0]}. Pinned rather than left red, so this file states a defect it
        # has measured instead of an outage it cannot fix. If Ghidra ever
        # corrects the module, this case goes red and says so.
        "module_defect_actual": VB_W[0] + VB_W[3] + VB_W[2] + VB_W[2],
    },
    {
        # vspltw vD,vB,uimm : every lane becomes vB.word[uimm]. Same family of
        # risk as vpermwi128 -- a lane index the module may read from the wrong
        # end. 0x8209CC44 is `vspltw v5,v13,0x2`.
        "name": "vspltw",
        "function": 0x8209CC44,
        "vec": {"vs45": VB},          # v13
        "capture": "vs37",            # v5
        "expect": VB_W[2] * 4,
        "module": True,
    },
    {
        # vmulfp128 vD,vA,vB : lane-wise float multiply. No lane order to get
        # wrong, which is exactly why it is worth checking -- it isolates
        # whether the arithmetic itself survives.
        # 0x8209CC54 is `vmulfp128 vr12,vr0,vr13`.
        "name": "vmulfp128",
        "function": 0x8209CC54,
        "vec": {
            "vr0":  "3f800000" "40000000" "40400000" "40800000",   # 1 2 3 4
            "vr13": "41200000" "41a00000" "41f00000" "42200000",   # 10 20 30 40
        },
        "capture": "vr12",
        # 10, 40, 90, 160
        "expect": "41200000" "42200000" "42b40000" "43200000",
        "module": True,
    },
    {
        # lvx128 vD,rA,rB : EA = (rA|0) + rB, then EA &= ~0xF, load 16 bytes.
        # 0x822A1EC0 is `lvx128 vr0,r0,r11`, so rA is r0 and the ISA reads it as
        # the literal zero.
        "name": "lvx128-aligned",
        "function": 0x822A1EC0,
        "gpr": {"r11": FIXTURE_BASE, "r0": 0},
        "capture": "vr0",
        "expect": FIXTURE[:32],
        "module": True,
    },
    {
        # The ~0xF mask: an address inside the second block must still load the
        # whole of that block from its start.
        "name": "lvx128-masked",
        "function": 0x822A1EC0,
        "gpr": {"r11": FIXTURE_BASE + 0x14, "r0": 0},
        "capture": "vr0",
        "expect": FIXTURE[32:],
        "module": True,
    },
    {
        # THE (rA|0) PROBE. Cycle 1296 read this module emitting
        # INT_ADD(r0, rB) with no (rA|0) rule and recorded it as latent because
        # r0 was zero on the path. Seeding r0 nonzero turns latent into measured:
        # the ISA ignores r0 entirely and must still load the first block.
        "name": "lvx128-ra-is-r0",
        "function": 0x822A1EC0,
        "gpr": {"r11": FIXTURE_BASE, "r0": 0x10},
        "capture": "vr0",
        "expect": FIXTURE[:32],
        "module": True,
        # If the module adds r0, it lands on the second block instead.
        "module_defect_actual": FIXTURE[32:],
    },
    {
        # vor vD,vA,vB with vA == vB is a register move. A weak test of the
        # operation and a real one of the byte order, which is why it is here.
        # 0x820A9AA4 is `vor v12,v13,v13`.
        "name": "vor",
        "function": 0x820A9AA4,
        "vec": {"vs45": VB},   # v13
        "capture": "vs44",     # v12
        "expect": VB,
        "module": True,
    },
    {
        # stvx128 vS,rA,rB : EA = (rA|0) + rB, EA &= ~0xF, store 16 bytes.
        # The only store in the closure, and the only case here checked through
        # memory_writes rather than a register. 0x822A1ECC is
        # `stvx128 vr0,r0,r11`; r11 points into a poison region so the write is
        # detected the same way every other write in this harness is.
        "name": "stvx128",
        "function": 0x822A1ECC,
        "gpr": {"r11": TARGET_BASE, "r0": 0},
        "vec": {"vr0": VB},
        "target": TARGET_BASE,
        "expect_write": VB,
        "module": True,
    },
    {
        # The same site with the module's p-code bypassed at the address. This
        # is the control for the override: it must produce the ISA answer, and
        # it must produce it through the module's own DECODE -- a wrong register
        # would read an unseeded one and the expectation would not match.
        "name": "vpermwi128-override",
        "function": 0x820A9BEC,
        "override": (0x820A9BEC, "vpermwi128"),
        "vec": {"vr12": VB},
        "capture": "vr13",
        "expect": VB_W[2] + VB_W[2] + VB_W[3] + VB_W[0],
    },
]

# 0x820A9B8C  lvlx v13,r0,r8 -> vs45 = CALLOTHER(r0, r8)
# Three alignments: none, mid-block, and one byte-group from the end. The third
# is the one that fails if the zero-fill is on the wrong side.
for _offset in (0, 4, 12):
    CASES.append({
        "name": f"lvlx+{_offset}",
        "function": 0x820A9B8C,
        "gpr": {"r8": FIXTURE_BASE + _offset},
        "capture": "vs45",
        "expect": expect_lvlx(_offset),
    })

CASES += MODULE_CASES

SPEC_HEADER = """\
# Generated by tools/audit_vmx128_behaviours.py --emit.
# One retail instruction, every input seeded, the output captured.
# Expected: 0x{expect}

function {function:#010x}
case vmx128:{name}
steps 1

region fixture {fixture_base:#010x} bytes:{fixture}
region target  {target_base:#010x} poison:0x40
region stack   0xC0000000 zero:0x1000
sp 0xC0000E00

vmx on
"""


def emit(workdir: Path) -> int:
    workdir.mkdir(parents=True, exist_ok=True)
    (workdir / "specs").mkdir(exist_ok=True)
    (workdir / "out").mkdir(exist_ok=True)
    manifest = []
    for case in CASES:
        lines = [SPEC_HEADER.format(expect=case.get("expect", case.get("expect_write")), function=case["function"],
                                    name=case["name"], fixture=FIXTURE,
                                    fixture_base=FIXTURE_BASE,
                                    target_base=TARGET_BASE)]
        for register, value in case.get("gpr", {}).items():
            lines.append(f"gpr {register} {value:#010x}")
        for register, value in case.get("vec", {}).items():
            lines.append(f"vec {register} {value}")
        if "override" in case:
            address, what = case["override"]
            lines.append(f"override {address:#010x} {what}")
        if "capture" in case:
            lines.append(f"capture vec:{case['capture']}")
        spec = workdir / "specs" / f"{case['name']}.spec"
        out = workdir / "out" / f"{case['name']}.json"
        spec.write_text("\n".join(lines) + "\n", encoding="utf-8")
        manifest.append(f"{spec.resolve()} {out.resolve()}")
    (workdir / "manifest").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"emitted cases={len(CASES)} manifest={workdir / 'manifest'}")
    return 0


def check(workdir: Path) -> int:
    passed, failures, defects = 0, [], []
    anchored = False
    for case in CASES:
        path = workdir / "out" / f"{case['name']}.json"
        if not path.exists():
            failures.append(f"{case['name']}: no snapshot")
            continue
        document = json.loads(path.read_text(encoding="utf-8"))
        fired = document["provenance"].get("asserted_semantics", {})
        if "expect_write" in case:
            runs = [w for w in document.get("memory_writes", [])
                    if int(w["address"], 16) == case["target"]]
            got_write = runs[0]["after_hex"] if runs else "<no write at the target>"
            if got_write != case["expect_write"]:
                failures.append(f"{case['name']}: wrote {got_write} "
                                f"want {case['expect_write']}")
                continue
            if fired:
                failures.append(f"{case['name']}: module case but a behaviour fired: {fired}")
                continue
            passed += 1
            continue
        got = document.get("registers", {}).get(case["capture"], "")
        want = "0x" + case["expect"]
        defect = case.get("module_defect_actual")
        if defect is not None:
            if got == "0x" + defect:
                defects.append(f"{case['name']}: module defect confirmed, "
                               f"got {got} where the ISA says {want}")
                passed += 1
            elif got == want:
                failures.append(f"{case['name']}: module now CORRECT ({got}); "
                                "remove the pinned defect and any override built on it")
            else:
                failures.append(f"{case['name']}: got {got}, neither the ISA answer "
                                f"{want} nor the pinned defect 0x{defect}")
            continue
        if got != want:
            failures.append(f"{case['name']}: got {got} want {want} (fired {fired})")
            continue
        # An operation that never fired cannot have been tested, and a captured
        # register that happens to hold the seed would pass silently. A
        # module-implemented instruction is the opposite case: nothing of ours
        # should fire, and if something did, the case is not testing what it says.
        if case.get("module"):
            if fired:
                failures.append(f"{case['name']}: module case but a behaviour fired: {fired}")
                continue
        elif not fired:
            failures.append(f"{case['name']}: matched but no behaviour fired")
            continue
        if case["name"] == "lvlx+0":
            anchored = True
        passed += 1
    print(f"cases={len(CASES)} passed={passed} failures={len(failures)} "
          f"pinned_module_defects={len(defects)}")
    # A lane-order verdict with no memory anchor is a statement about my own
    # convention, not about the module. Refuse to publish one.
    if defects and not anchored:
        failures.append("lane-order defects reported but the memory anchor "
                        "(lvlx+0) did not pass: the convention is unproven")
        defects = []
    for defect in defects:
        print(f"  DEFECT {defect}")
    for failure in failures:
        print(f"  {failure}", file=sys.stderr)
    if failures:
        print("vmx128_behaviours=fail", file=sys.stderr)
        return 1
    print("vmx128_behaviours=pass")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--workdir", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--emit", action="store_true")
    mode.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    return emit(arguments.workdir) if arguments.emit else check(arguments.workdir)


if __name__ == "__main__":
    raise SystemExit(main())
