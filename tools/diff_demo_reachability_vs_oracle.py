#!/usr/bin/env python3
"""Rank where the native demo run stops following the retail runtime.

Xenia writes one HIR dump per guest function it translates, and it translates
on first execution, so the dump directory of an oracle run is a reachability
set: these functions ran. `ac6-demo-recomp probe --atlas` produces the same
set for the native run. Subtracting one from the other says how far the port
gets; intersecting the difference with the static call graph says *where* it
stops, which is the part worth acting on.

The oracle is used only to decide what to look at. No behaviour, value or
address is taken from it.

Two things are filtered, both because they would otherwise dominate the list:

  - the PPC ABI save/restore helper families, which the port inlines rather
    than calls, so they can never appear in a native atlas;
  - functions the native run reaches, since by construction they are not
    divergences.

usage:
  diff_demo_reachability_vs_oracle.py ORACLE_LIST NATIVE_ATLAS.json [--top N]

ORACLE_LIST is any text listing the oracle's HIR dump paths or bare
eight-digit addresses, e.g. the output of `tar -tf` over the decomp archive.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ADDRESS = re.compile(r"\b([0-9A-Fa-f]{8})\b")
ATLAS = Path("analysis/demo/ac6-demo-static-decomp-atlas-v1.json")
# __savegprlr_*/__restgprlr_* and the FPR/VMX equivalents.
HELPERS = ((0x82327000, 0x823271FF), (0x82375A00, 0x82376FFF))


def is_helper(address: int) -> bool:
    return any(low <= address <= high for low, high in HELPERS)


def oracle_set(path: Path) -> set[int]:
    found = set()
    for line in path.read_text(errors="replace").splitlines():
        for match in ADDRESS.finditer(line.rsplit("/", 1)[-1]):
            value = int(match.group(1), 16)
            if 0x82000000 <= value < 0x82400000:
                found.add(value)
    return found


def native_set(path: Path) -> dict[int, int]:
    document = json.loads(path.read_text())
    return {
        int(entry["address"], 16): entry.get("count", 0)
        for entry in document.get("functions", [])
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("atlas", type=Path)
    parser.add_argument("--static-atlas", type=Path, default=ATLAS)
    parser.add_argument("--top", type=int, default=20)
    arguments = parser.parse_args()

    oracle = oracle_set(arguments.oracle)
    native = native_set(arguments.atlas)
    if not oracle or not native:
        print("reachability_diff=fail reason=empty input", file=sys.stderr)
        return 1
    missing = {a for a in oracle - set(native) if not is_helper(a)}
    surplus = {a for a in set(native) - oracle if not is_helper(a)}
    print(f"oracle={len(oracle)} native={len(native)} "
          f"common={len(oracle & set(native))} missing={len(missing)} "
          f"native_only={len(surplus)}")
    if surplus:
        # The port reaching something the oracle never did would mean one of
        # the two sets is not what it is taken for; say so rather than rank.
        print("warning: native_only is not empty: "
              + ",".join(hex(a) for a in sorted(surplus)[:8]))

    static = json.loads(arguments.static_atlas.read_text())
    frontier = []
    for function in static.get("functions", []):
        entry = int(function["entry"], 16)
        if entry not in native:
            continue
        stops = sorted(
            int(callee, 16)
            for callee in (function.get("direct_calls") or [])
            if int(callee, 16) in missing
        )
        if stops:
            frontier.append((native[entry], entry, stops))
    frontier.sort(reverse=True)
    print(f"frontier={len(frontier)}")
    for count, entry, stops in frontier[: arguments.top]:
        listed = ",".join(hex(a) for a in stops[:5])
        print(f"  {entry:#010x} calls={count} stops={len(stops)} -> {listed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
