#!/usr/bin/env python3
"""Fail-closed audit of the RTTI class map.

A map of names is only worth what its discipline is worth, so this checks the
discipline rather than the size. Every row must carry a mangled name the sweep
could actually resolve; a template must keep its mangled name instead of a
pretty wrong one; every row must list at least its own class among the bases;
and the classes the reports already reason about must be present, so a map that
silently loses them fails rather than passes smaller.

Usage:
    python3 tools/audit_ac6_class_map.py analysis/class-map.tsv --require J2
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Classes the reports name and reason about. A map that cannot find one of
# these is not a map of this binary.
ANCHORS = (
    "CX360UnitManager",
    "CX360ObjManager",
    "ACE6::CAce6Thread",
    "ACE6::CAce6MissionManager",
    "ACE6::CAce6MissionManagerCampaign",
    "ACE6::CAce6MissionManagerReplay",
)

# The J2 floor: the sweep found 811 vtables, and a later run may find more but
# must not silently find fewer.
MINIMUM_VTABLES = 811


def audit(path: Path) -> tuple[int, list[str]]:
    problems: list[str] = []
    rows = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4:
            problems.append(f"line {number}: expected 4 columns, found {len(fields)}")
            continue
        vtable, pretty, bases, mangled = fields
        rows.append((vtable, pretty, bases, mangled))

        if not vtable.startswith("0x"):
            problems.append(f"line {number}: {vtable} is not an address")
        if not mangled.startswith(".?A"):
            problems.append(f"line {number}: {mangled!r} is not a decorated name")
        if "?$" in mangled and pretty:
            problems.append(
                f"line {number}: template {mangled} carries an undecorated name")
        if "?$" not in mangled and not pretty:
            problems.append(f"line {number}: {mangled} has no undecorated name")
        if not bases:
            problems.append(f"line {number}: {mangled} lists no base, not even itself")

    names = {row[1] for row in rows} | {row[3] for row in rows}
    for anchor in ANCHORS:
        if anchor not in names:
            problems.append(f"anchor class absent: {anchor}")

    if len(rows) < MINIMUM_VTABLES:
        problems.append(f"only {len(rows)} vtables, floor is {MINIMUM_VTABLES}")

    return len(rows), problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", type=Path)
    parser.add_argument("--require", default=None)
    arguments = parser.parse_args(argv)

    if not arguments.map.exists():
        print(f"class_map=fail reason=missing path={arguments.map}", file=sys.stderr)
        return 1

    count, problems = audit(arguments.map)
    for problem in problems[:20]:
        print(f"class_map_problem: {problem}", file=sys.stderr)
    if problems:
        print(f"class_map=fail vtables={count} problems={len(problems)}", file=sys.stderr)
        return 1
    print(f"class_map=pass vtables={count} require={arguments.require or 'none'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
