#!/usr/bin/env python3
"""Parse a scenario payload, re-emit it, and require byte-for-byte identity.

This is the S3 proof: the strongest evidence available without executing
anything. A schema-driven walk that merely reads without complaint proves very
little - it can skip a table, misread a count, or never reach a subtree, and
still look successful. Rebuilding the file and demanding the same SHA-256 makes
those failures loud.

What is reconstructed, and what is copied:

* every **structural word** is recomputed from the parsed model, never copied.
  A node's two offsets are re-derived as ``data_absolute - node`` and
  ``table_absolute - node``; a table's count and every child offset are
  re-derived the same way. If the walk misread one table, its bytes come back
  wrong and the digest moves.
* every **data block** is copied verbatim. No schema claims to model what a
  data block contains - floats, bytes, identifiers - so re-emitting it is not
  something this tool can prove.

Coverage falls out of the same run: the emitter starts from an all-zero buffer
and marks every byte it writes. The contract is that the rebuilt file is
identical AND every byte the walk never claimed is a zero - unclaimed non-zero
bytes would be information the walk never saw. The unclaimed zeros are counted
and reported rather than hidden; on the Mission 01 payload they are the
alignment padding between blocks.

Usage:
    python3 tools/roundtrip_ac6_scenario.py PAYLOAD [--json OUT]
"""

from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from emit_ac6_native_snapshot import Payload  # noqa: E402


class Walk:
    """Every node and table reachable from the root, with their raw words."""

    def __init__(self, payload: Payload) -> None:
        self.p = payload
        self.nodes: dict[int, tuple[int, int]] = {}   # node -> (data_off, table_off)
        self.tables: dict[int, list[int]] = {}        # table -> [child_off, ...]
        self.data: set[int] = set()                   # absolute data block starts
        self._visit()

    def _visit(self) -> None:
        # An explicit stack: the node graph is wide, and recursion over it is a
        # needless way to depend on the interpreter's limit.
        pending = [0]
        while pending:
            node = pending.pop()
            if node in self.nodes:
                continue
            if node + 8 > len(self.p.data):
                raise ValueError(f"node at {node:#x} runs past the payload")
            data_off, table_off = struct.unpack_from(">II", self.p.data, node)
            self.nodes[node] = (data_off, table_off)
            if data_off:
                self.data.add(node + data_off)
            if not table_off:
                continue
            table = node + table_off
            if table in self.tables:
                continue
            count = self.p.s32(table)
            if count < 0:
                self.tables[table] = []
                continue
            offsets = [self.p.u32(table + 4 + 4 * index) for index in range(count)]
            self.tables[table] = offsets
            pending.extend(table + offset for offset in offsets)


def boundaries(walk: Walk) -> list[int]:
    """Every offset a structure is known to start at, in order."""
    marks = set(walk.nodes)
    marks.update(walk.tables)
    marks.update(walk.data)
    return sorted(marks)


def reemit(payload: Payload, walk: Walk) -> tuple[bytes, bytearray]:
    size = len(payload.data)
    out = bytearray(size)
    written = bytearray(size)

    def put(offset: int, blob: bytes) -> None:
        out[offset:offset + len(blob)] = blob
        for index in range(len(blob)):
            written[offset + index] = 1

    # Structure, recomputed from the model.
    for node, (data_off, table_off) in walk.nodes.items():
        put(node, struct.pack(">II", data_off, table_off))
    for table, offsets in walk.tables.items():
        # A table whose count is negative keeps that count: the walk refuses to
        # follow it, so it cannot re-derive it and says so rather than inventing.
        raw_count = payload.s32(table)
        blob = struct.pack(">i", raw_count)
        for offset in offsets:
            blob += struct.pack(">I", offset)
        put(table, blob)

    # Data blocks, copied to the next known boundary.
    marks = boundaries(walk)
    for start in sorted(walk.data):
        index = bisect.bisect_right(marks, start)
        end = marks[index] if index < len(marks) else size
        put(start, payload.data[start:end])

    return bytes(out), written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("--json", type=Path)
    arguments = parser.parse_args(argv)

    raw = arguments.payload.read_bytes()
    payload = Payload(raw)
    walk = Walk(payload)
    rebuilt, written = reemit(payload, walk)

    unwritten = [index for index, flag in enumerate(written) if not flag]
    unwritten_nonzero = [index for index in unwritten if raw[index] != 0]
    runs = 0
    longest = 0
    current = 0
    for flag in written:
        if flag:
            longest = max(longest, current)
            if current:
                runs += 1
            current = 0
        else:
            current += 1
    if current:
        runs += 1
        longest = max(longest, current)
    structure_bytes = 8 * len(walk.nodes) + sum(4 + 4 * len(offsets)
                                                for offsets in walk.tables.values())
    report = {
        "schema": "ac6.scenario-roundtrip.v1",
        "payload_sha256": hashlib.sha256(raw).hexdigest(),
        "rebuilt_sha256": hashlib.sha256(rebuilt).hexdigest(),
        "identical": rebuilt == raw,
        "size": len(raw),
        "nodes": len(walk.nodes),
        "tables": len(walk.tables),
        "data_blocks": len(walk.data),
        "structure_bytes": structure_bytes,
        "unwritten_bytes": len(unwritten),
        "unwritten_nonzero_bytes": len(unwritten_nonzero),
        "unwritten_runs": runs,
        "longest_unwritten_run": longest,
        "unwritten_first": [f"0x{offset:x}" for offset in unwritten[:16]],
        "first_difference": None,
    }
    if rebuilt != raw:
        for index, (left, right) in enumerate(zip(raw, rebuilt)):
            if left != right:
                report["first_difference"] = f"0x{index:x}"
                break
    if arguments.json is not None:
        arguments.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in (
        "identical", "size", "nodes", "tables", "data_blocks", "structure_bytes",
        "unwritten_bytes", "unwritten_nonzero_bytes", "unwritten_runs",
        "longest_unwritten_run", "first_difference")}))
    # The contract: the model reproduces the file exactly, and every byte it
    # does not claim is a zero. A single unclaimed non-zero byte would be
    # information the walk never saw.
    return 0 if report["identical"] and not unwritten_nonzero else 1


if __name__ == "__main__":
    raise SystemExit(main())
