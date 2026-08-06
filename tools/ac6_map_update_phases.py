#!/usr/bin/env python3
"""Map AC6 update profiler labels to exact PAL instruction addresses.

The input is read-only, revision-pinned generated C++ used strictly as a
literal instruction-stream cross-match after qualification of the PAL XEX.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


PHASES = {
    0x82008208: "UpBegin",
    0x820081FC: "UpSpecial",
    0x820081F0: "UpLocaUnit",
    0x820081E4: "UpLocaObj",
    0x820081D8: "UpLocaDb",
    0x820081CC: "UpLocaArms",
    0x820081C4: "UpFlag",
    0x820081BC: "UpInput",
    0x820081AC: "UpObj",
    0x820081A4: "UpRep",
    0x8200819C: "UpArms",
    0x82008194: "UpCam",
    0x8200818C: "UpMap",
    0x82008184: "UpHud",
    0x8200817C: "UpRadio",
    0x82008170: "UpSubWin",
    0x82008168: "UpEff",
}

EXPECTED_LABEL_PCS = {
    "UpBegin": 0x8226D298,
    "UpSpecial": 0x8226D32C,
    "UpLocaUnit": 0x8226D73C,
    "UpLocaObj": 0x8226D774,
    "UpLocaDb": 0x8226D7D8,
    "UpLocaArms": 0x8226D810,
    "UpFlag": 0x8226D84C,
    "UpInput": 0x8226D9C4,
    "UpObj": 0x8226DA84,
    "UpRep": 0x8226DD0C,
    "UpArms": 0x8226DD54,
    "UpCam": 0x8226DD9C,
    "UpMap": 0x8226DE00,
    "UpHud": 0x8226DF00,
    "UpRadio": 0x8226DF58,
    "UpSubWin": 0x8226DFA0,
    "UpEff": 0x8226E054,
}


def parse_instructions(source: str, symbol: str, start_pc: int) -> list[tuple[int, str]]:
    marker = f"PPC_FUNC_IMPL(__imp__{symbol})"
    begin = source.index(marker)
    end = source.index("\n__attribute__((alias(", begin + len(marker))
    instructions: list[tuple[int, str]] = []
    pc = start_pc
    for line in source[begin:end].splitlines():
        match = re.match(r"\s*//\s+(.+)$", line)
        if not match:
            continue
        instructions.append((pc, match.group(1)))
        pc += 4
    return instructions


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("generated_cpp", type=Path)
    parser.add_argument("--context", type=int, default=10)
    parser.add_argument("--dump-phase", action="append", default=[])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    instructions = parse_instructions(
        args.generated_cpp.read_text(encoding="utf-8"), "sub_8226D1C8", 0x8226D1C8
    )
    observed: dict[str, int] = {}
    for index, (pc, text) in enumerate(instructions):
        match = re.fullmatch(r"addi r4,r11,(-?\d+)", text)
        if not match:
            continue
        address = 0x82010000 + int(match.group(1))
        phase = PHASES.get(address)
        if phase is None:
            continue
        observed[phase] = pc
        print(f"{phase}\tlabel=0x{address:08X}\tlabel_pc=0x{pc:08X}")
        lower = max(0, index - args.context)
        upper = min(len(instructions), index + args.context + 1)
        dump_all = phase in args.dump_phase
        for nearby_pc, nearby_text in instructions[lower:upper]:
            if dump_all or nearby_text.startswith(("bl ", "bctrl")):
                kind = "pc" if dump_all else "call_pc"
                print(f"  {kind}=0x{nearby_pc:08X}\t{nearby_text}")
    if args.check and observed != EXPECTED_LABEL_PCS:
        missing = sorted(set(EXPECTED_LABEL_PCS) - set(observed))
        extra = sorted(set(observed) - set(EXPECTED_LABEL_PCS))
        wrong = sorted(
            phase
            for phase in set(observed) & set(EXPECTED_LABEL_PCS)
            if observed[phase] != EXPECTED_LABEL_PCS[phase]
        )
        print(f"phase contract mismatch: missing={missing} extra={extra} wrong={wrong}")
        return 1
    if args.check:
        print(f"phase contract ok: {len(observed)} labels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
