#!/usr/bin/env python3
"""Diff two guest-store captures and name the code that writes what only one press writes.

Each capture is an array of (address, writer_lr, value) records produced by the
P2.3 store watchpoint: every store the recompiled guest performed in the window
following an input edge, tagged with the guest lr of the instruction that made
it. Both fields are guest values, so what is printed can be looked up directly
in Ghidra or in the generated corpus -- no load slide, no symbol table.

This is the instrument cycle 440 named and nobody built. Snapshot diffing, which
was done instead for twelve cycles, reports that a word changed; it never
reports who changed it, and it cannot see a field written back to the same value
inside a frame. Both limits disappear here.

Usage:
    tools/ac6-store-diff.py --a stores-right.bin --b stores-a.bin \\
        [--label-a Right] [--label-b A] [--guest-only]

Reports:
  * writers active only in A, and only in B
  * addresses written only by A, and only by B
  * for the most interesting writers, the addresses they touch
"""

import argparse
import struct
from collections import Counter, defaultdict

RECORD = struct.Struct("<IIQ")  # address, writer lr, value


def load(path):
    with open(path, "rb") as handle:
        data = handle.read()
    count = len(data) // RECORD.size
    return [RECORD.unpack_from(data, i * RECORD.size) for i in range(count)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--a", required=True, help="capture of the press that works")
    parser.add_argument("--b", required=True, help="capture of the press that does not")
    parser.add_argument("--label-a", default="A")
    parser.add_argument("--label-b", default="B")
    parser.add_argument("--top", type=int, default=25)
    parser.add_argument(
        "--guest-only",
        action="store_true",
        help="ignore stores below 0x82000000 (stack and scratch), keeping image "
             "and heap writes -- the ones that can hold UI state",
    )
    args = parser.parse_args()

    rec_a, rec_b = load(args.a), load(args.b)

    def keep(record):
        return not args.guest_only or record[0] >= 0x82000000

    rec_a = [r for r in rec_a if keep(r)]
    rec_b = [r for r in rec_b if keep(r)]

    print(f"{args.label_a}: {len(rec_a)} stores")
    print(f"{args.label_b}: {len(rec_b)} stores\n")

    writers_a = Counter(r[1] for r in rec_a)
    writers_b = Counter(r[1] for r in rec_b)
    only_wa = sorted(set(writers_a) - set(writers_b), key=lambda w: -writers_a[w])
    only_wb = sorted(set(writers_b) - set(writers_a), key=lambda w: -writers_b[w])

    print(f"=== writers active only in {args.label_a} ({len(only_wa)}) ===")
    for w in only_wa[: args.top]:
        print(f"  lr=0x{w:08X}  {writers_a[w]} stores")
    if len(only_wa) > args.top:
        print(f"  ... {len(only_wa) - args.top} more")

    print(f"\n=== writers active only in {args.label_b} ({len(only_wb)}) ===")
    for w in only_wb[: args.top]:
        print(f"  lr=0x{w:08X}  {writers_b[w]} stores")
    if len(only_wb) > args.top:
        print(f"  ... {len(only_wb) - args.top} more")

    addrs_a = {r[0] for r in rec_a}
    addrs_b = {r[0] for r in rec_b}
    only_aa = sorted(addrs_a - addrs_b)
    only_ab = sorted(addrs_b - addrs_a)
    print(f"\n=== addresses written only by {args.label_a}: {len(only_aa)} ===")
    by_writer = defaultdict(set)
    for address, writer, _ in rec_a:
        if address in (addrs_a - addrs_b):
            by_writer[writer].add(address)
    for writer in sorted(by_writer, key=lambda w: -len(by_writer[w]))[: args.top]:
        sample = sorted(by_writer[writer])[:6]
        pretty = " ".join(f"0x{a:08X}" for a in sample)
        print(f"  lr=0x{writer:08X}  {len(by_writer[writer])} addresses  {pretty}")

    print(f"\n=== addresses written only by {args.label_b}: {len(only_ab)} ===")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
