#!/usr/bin/env python3
"""Find the next AC6 runtime blocker and the config split that causes it.

Background
----------
The AC6 native runtime aborts inside recompiled PPC code on a REX_FATAL that
ReXGlue emits for a branch whose target is not an addressable label:

    // ERROR: conditional branch to unknown address 0x823452A0
    if (!ctx.cr0.eq) REX_FATAL("Unresolved branch from 0x823452CC to 0x823452A0");

This happens when a backward branch crosses a declared function boundary that
is not a real function start. `sub_82345100`, for example, is one function
fragmented by four declared entries; removing all four cleared every trap in it
and let execution advance to an entirely different call chain.

The fix loop is mechanical, and this script performs its analysis half:

  1. run the runtime under gdb, capture the REX_FATAL that fires;
  2. find the `[functions]` entry strictly between target and source;
  3. that entry is the removal candidate;
  4. remove, regenerate, rebuild, repeat.

Measured on 2026-07-26: traps 4857 -> 4838 (four splits in sub_82345100),
then -> 4836 (0x82349050 in sub_82348FC8).

Read-only: it never edits the config. It reports the candidate; removing it is
a decision that still needs a headless boundary contract before promotion.

Usage:
    python3 ac6-next-split.py --run       # run under gdb, then analyse
    python3 ac6-next-split.py --trace F   # analyse an existing gdb log
    python3 ac6-next-split.py --count     # just count traps in generated/
"""
from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

REF = Path("/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference")
CONFIG = REF / "ac6recomp_config.toml"
GENERATED = REF / "generated"
BINARY = REF / "out/build/linux-amd64-runtime-localdev/ac6recomp"

FATAL = re.compile(r'REX_FATAL\("Unresolved branch from 0x([0-9A-F]+) to 0x([0-9A-F]+)"\)')
FRAME = re.compile(r'#\d+\s+0x[0-9a-f]+ in __imp__((?:rex_)?sub_[0-9A-F]+).*?:(\d+)')
ENTRY = re.compile(r'^0x([0-9A-F]{8}) = \{', re.M)


def entries() -> list[int]:
    return sorted(int(a, 16) for a in ENTRY.findall(CONFIG.read_text(encoding="utf-8")))


def count_traps() -> int:
    n = 0
    for p in GENERATED.glob("*.cpp"):
        n += len(FATAL.findall(p.read_text(encoding="utf-8", errors="replace")))
    return n


def run_under_gdb(timeout: int) -> str:
    cmd = ["timeout", str(timeout), "xvfb-run", "-a", "gdb", "-batch",
           "-ex", "run", "-ex", "bt 12", "--args", str(BINARY)]
    return subprocess.run(cmd, capture_output=True, text=True, cwd=REF).stdout


def analyse(trace: str) -> int:
    frames = FRAME.findall(trace)
    if not frames:
        print("no recompiled frame in the trace; the abort may be host-side")
        return 1
    func, line = frames[0]
    print(f"aborting function : __imp__{func}")
    print(f"generated line    : {line}")

    src = tgt = None
    for p in GENERATED.glob("*.cpp"):
        text = p.read_text(encoding="utf-8", errors="replace").splitlines()
        idx = int(line) - 1
        if 0 <= idx < len(text):
            m = FATAL.search(text[idx])
            if m:
                src, tgt = int(m.group(1), 16), int(m.group(2), 16)
                print(f"file              : {p.name}")
                break
    if src is None:
        print("could not locate the REX_FATAL at that line; widen the search")
        return 1

    print(f"unresolved branch : {src:#010x} -> {tgt:#010x}")
    addrs = entries()
    if tgt in addrs:
        # Target is itself a declared entry: it must be removed, or the
        # generated code emits `goto loc_<target>` with no matching label.
        between = [tgt]
    else:
        between = [a for a in addrs if min(src, tgt) < a <= max(src, tgt)]
    owner = [a for a in addrs if a <= min(src, tgt)]
    print(f"function owning target: {owner[-1]:#010x}" if owner else "no owner found")
    print()
    if not between:
        print("NO split between target and source -- this trap has a different cause.")
        return 2
    print(f"removal candidate{'s' if len(between) > 1 else ''}:")
    for a in between:
        print(f"  0x{a:08X}")
    print("\nNot qualified: each still needs a headless boundary contract before promotion.")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--run", action="store_true", help="run the binary under gdb first")
    ap.add_argument("--trace", help="analyse an existing gdb trace file")
    ap.add_argument("--count", action="store_true", help="count traps and exit")
    ap.add_argument("--timeout", type=int, default=150)
    a = ap.parse_args()

    if a.count:
        print(f"unresolved-branch traps in generated/: {count_traps()}")
        return 0
    if a.trace:
        return analyse(Path(a.trace).read_text(encoding="utf-8", errors="replace"))
    if a.run:
        print(f"traps before: {count_traps()}")
        return analyse(run_under_gdb(a.timeout))
    ap.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
