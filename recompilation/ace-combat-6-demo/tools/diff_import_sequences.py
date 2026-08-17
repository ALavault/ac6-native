#!/usr/bin/env python3
"""Diff the native import journal against the Xenia oracle kernel-call log.

The native side is AC6_DEMO_WATCH_IMPORTS=1 stderr (AC6_IMPORT_CALL lines).
The oracle side is a Xenia debug log recorded by run_xenia_kernel_log.sh.

Xenia logs no return values and no guest thread id that maps onto ours, so the
only sound comparisons are the ordered sequence of export names and the set of
names each side ever calls. That is enough for the question this exists to
answer: which kernel/XAM call does the retail runtime make that the native
runtime never does?
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

CALLGRAPH = (Path(__file__).resolve().parents[3]
             / "analysis/demo/ac6-demo-sdk-callgraph.json")

NATIVE = re.compile(r"^AC6_IMPORT_CALL\b.*?\bname=(\S+)")
# Xenia's PrintKernelCall appends "Name(args)"; the log prefix before it varies
# by build, so anchor on the last "Identifier(" of the line.
ORACLE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\(")


def native_names(path: Path) -> list[str]:
    out = []
    for line in path.read_text(errors="replace").splitlines():
        found = NATIVE.match(line)
        if found:
            out.append(found.group(1))
    return out


def oracle_names(path: Path, known: set[str]) -> list[str]:
    """Keep only identifiers the native side can also name.

    Without this the regex also matches ordinary parenthesised words in
    Xenia's non-kernel log lines. Restricting to the union of names either
    side knows keeps the sequence comparable instead of merely long.
    """
    out = []
    for line in path.read_text(errors="replace").splitlines():
        for name in ORACLE.findall(line):
            if name in known:
                out.append(name)
    return out


def first_divergence(a: list[str], b: list[str]) -> int | None:
    for index, (left, right) in enumerate(zip(a, b)):
        if left != right:
            return index
    return None if len(a) == len(b) else min(len(a), len(b))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("--context", type=int, default=5)
    parser.add_argument("--names", type=Path, default=CALLGRAPH,
                        help="the guest's import table; a .json callgraph or a "
                             "newline-separated name list. This is the universe "
                             "of comparable names -- it must NOT default to the "
                             "names the native side happened to call, or the "
                             "imports it never calls, which are the answer this "
                             "tool exists to find, would be filtered out.")
    arguments = parser.parse_args()

    native = native_names(arguments.native)
    known = set(native)
    if arguments.names and arguments.names.is_file():
        text = arguments.names.read_text()
        if arguments.names.suffix == ".json":
            known |= {
                record["name"]
                for record in json.loads(text).get("imports", [])
                if isinstance(record, dict) and isinstance(record.get("name"), str)
            }
        else:
            known |= {line.strip() for line in text.splitlines() if line.strip()}
    oracle = oracle_names(arguments.oracle, known)

    print(f"native_calls={len(native)} oracle_calls={len(oracle)}")
    native_counts = collections.Counter(native)
    oracle_counts = collections.Counter(oracle)
    only_oracle = sorted(set(oracle_counts) - set(native_counts))
    only_native = sorted(set(native_counts) - set(oracle_counts))
    print(f"only_oracle={','.join(only_oracle) or '-'}")
    print(f"only_native={','.join(only_native) or '-'}")

    index = first_divergence(native, oracle)
    if index is None:
        print("first_divergence=none")
        return 0
    low = max(0, index - arguments.context)
    print(f"first_divergence={index}")
    print(f"  native: {' '.join(native[low:index + arguments.context])}")
    print(f"  oracle: {' '.join(oracle[low:index + arguments.context])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
