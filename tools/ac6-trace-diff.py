#!/usr/bin/env python3
"""Resolve and diff two guest control-flow traces captured by ac6_guest_call_trace.

The traces are raw arrays of host function pointers, written by the runtime with
AC6RECOMP_TRACE_GUEST_CALLS. Resolving them here rather than in-process keeps the
frame loop cheap and makes the resolution exact: each address is matched to the
symbol that starts at or below it, using the binary's own symbol table.

Usage:
    tools/ac6-trace-diff.py --binary build-instr/ac6recomp \\
        --a trace-right.bin --b trace-a.bin \\
        [--label-a Right] [--label-b A]

Reports, in order of usefulness:
  * functions reached only by A (the working press) -- the candidate consumers
  * functions reached only by B
  * the first point at which the two execution paths diverge

The point of the exercise: with one press that provably changes the frame and
one that provably does not, a function present in exactly one trace is the
place the two paths part company.
"""

import argparse
import bisect
import struct
import subprocess
import sys
from collections import Counter


def load_symbols(binary):
    """Sorted (address, name) for every text symbol in the binary."""
    out = subprocess.run(["nm", "-n", binary], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue
        addr, kind, name = parts
        if kind not in "tTwW":
            continue
        try:
            syms.append((int(addr, 16), name))
        except ValueError:
            continue
    syms.sort()
    return syms


def load_trace(path):
    with open(path, "rb") as handle:
        data = handle.read()
    count = len(data) // 8
    return struct.unpack(f"<{count}Q", data[: count * 8])


def resolve_all(trace, addrs, names):
    """Map each recorded pointer to its containing symbol name."""
    cache = {}
    resolved = []
    for value in trace:
        name = cache.get(value)
        if name is None:
            index = bisect.bisect_right(addrs, value) - 1
            name = names[index] if index >= 0 else f"?{value:#x}"
            cache[value] = name
        resolved.append(name)
    return resolved


def guest_name(symbol):
    """Strip the recompiler's wrappers so sub_XXXXXXXX reads plainly."""
    for prefix in ("__imp__rex_", "__imp__", "rex_"):
        if symbol.startswith(prefix):
            return symbol[len(prefix):]
    return symbol


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--a", required=True, help="trace of the press that works")
    parser.add_argument("--b", required=True, help="trace of the press that does not")
    parser.add_argument("--label-a", default="A")
    parser.add_argument("--label-b", default="B")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument(
        "--reference-a",
        help="hex runtime address of __cyg_profile_func_enter for trace A",
    )
    parser.add_argument(
        "--reference-b",
        help="hex runtime address of __cyg_profile_func_enter for trace B. Each "
             "run has its own ASLR slide, so a single value applied to both "
             "traces resolves one of them to garbage.",
    )
    parser.add_argument(
        "--reference-runtime",
        help="hex runtime address of __cyg_profile_func_enter, printed in the "
             "[ac6-trace] armed line. The binary is position-independent, so "
             "without this every entry resolves to the lowest symbol and the "
             "two traces compare as identical.",
    )
    args = parser.parse_args()

    syms = load_symbols(args.binary)
    if not syms:
        print(f"no symbols in {args.binary}", file=sys.stderr)
        return 2
    addrs = [a for a, _ in syms]
    names = [n for _, n in syms]

    link = next((a for a, n in syms if n == "__cyg_profile_func_enter"), None)

    def slide_for(reference):
        if not reference:
            return 0
        if link is None:
            print("__cyg_profile_func_enter not in the symbol table", file=sys.stderr)
            sys.exit(2)
        return int(reference, 16) - link

    slide_a = slide_for(args.reference_a or args.reference_runtime)
    slide_b = slide_for(args.reference_b or args.reference_runtime)
    print(f"load slide A: {slide_a:#x}\nload slide B: {slide_b:#x}\n")

    trace_a = resolve_all([v - slide_a for v in load_trace(args.a)], addrs, names)
    trace_b = resolve_all([v - slide_b for v in load_trace(args.b)], addrs, names)

    count_a = Counter(guest_name(s) for s in trace_a)
    count_b = Counter(guest_name(s) for s in trace_b)
    set_a, set_b = set(count_a), set(count_b)

    print(f"{args.label_a}: {len(trace_a)} entries, {len(set_a)} distinct functions")
    print(f"{args.label_b}: {len(trace_b)} entries, {len(set_b)} distinct functions")
    print(f"shared: {len(set_a & set_b)} functions\n")

    only_a = sorted(set_a - set_b, key=lambda n: -count_a[n])
    only_b = sorted(set_b - set_a, key=lambda n: -count_b[n])

    print(f"=== reached only by {args.label_a} ({len(only_a)}) ===")
    for name in only_a[: args.top]:
        print(f"  {name:<28} {count_a[name]}")
    if len(only_a) > args.top:
        print(f"  ... {len(only_a) - args.top} more")

    print(f"\n=== reached only by {args.label_b} ({len(only_b)}) ===")
    for name in only_b[: args.top]:
        print(f"  {name:<28} {count_b[name]}")
    if len(only_b) > args.top:
        print(f"  ... {len(only_b) - args.top} more")

    # Where the two paths part company. Both captures start at the same point in
    # the frame -- the pad edge -- so a common prefix is expected, and the first
    # index where they differ is the branch that the two presses take
    # differently.
    limit = min(len(trace_a), len(trace_b))
    split = next((i for i in range(limit)
                  if guest_name(trace_a[i]) != guest_name(trace_b[i])), limit)
    print(f"\n=== first divergence at entry {split} ===")
    lo = max(0, split - 4)
    for i in range(lo, min(limit, split + 6)):
        mark = ">>" if i == split else "  "
        print(f"  {mark} [{i}] {guest_name(trace_a[i]):<28} | {guest_name(trace_b[i])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
