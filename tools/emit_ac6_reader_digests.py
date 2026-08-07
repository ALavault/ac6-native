#!/usr/bin/env python3
"""Reduce the committed p-code snapshots to one digest per case.

The C++ port of the *Bin readers has to be checked against something. The
strongest available reference is not the Python parser but the snapshots the
Ghidra p-code emulator produced by executing the retail instructions
themselves: ``analysis/microexec/**/*.ppc.json``. This tool reduces each of
them to a single 64-bit digest over its written memory, so a native test can
replay the same node and compare without parsing JSON.

The digest is defined on the canonical serialization of the written runs, in
address order:

    "{address:08x}:{size}:{after_hex}\\n"  concatenated, hashed with FNV-1a 64

Both sides must agree on that string exactly, which is why it is spelled out
here and in the C++ reader.

Usage:
    python3 tools/emit_ac6_reader_digests.py analysis/microexec \\
        --output analysis/microexec/reader-digests.tsv
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

CASE = re.compile(r"^(?P<klass>[A-Za-z0-9]+)@node\+(?P<offset>0x[0-9a-fA-F]+)$")

HEADER = """\
# One digest per committed p-code snapshot, so a native reader can be replayed
# against the retail parser's own writes without parsing JSON.
# Produced by tools/emit_ac6_reader_digests.py from analysis/microexec/**.ppc.json.
# digest = FNV-1a 64 over "{address:08x}:{size}:{after_hex}\\n" per written run,
# in address order.
# columns: class node_offset run_count written_bytes digest
"""


def fnv64(text: str) -> int:
    digest = 0xCBF29CE484222325
    for byte in text.encode("ascii"):
        digest ^= byte
        digest = (digest * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return digest


def canonical(writes: list[dict]) -> tuple[str, int, int]:
    """The serialization both sides hash, plus the run and byte counts."""
    runs = sorted(writes, key=lambda run: int(run["address"], 16))
    text = ""
    written = 0
    for run in runs:
        address = int(run["address"], 16)
        payload = run["after_hex"]
        size = int(run["size"])
        if len(payload) != 2 * size:
            raise ValueError(f"run at {address:#x} declares {size} bytes, has {len(payload) // 2}")
        text += f"{address:08x}:{size}:{payload}\n"
        written += size
    return text, len(runs), written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)

    rows: list[tuple[str, int, int, int, int]] = []
    for path in sorted(arguments.root.rglob("*.ppc.json")):
        document = json.loads(path.read_text(encoding="utf-8"))
        identity = document.get("identity", {})
        if identity.get("implementation") != "ppc-pcode":
            continue
        match = CASE.match(str(identity.get("case", "")))
        if match is None:
            raise SystemExit(f"{path}: unreadable case {identity.get('case')!r}")
        text, run_count, written = canonical(document.get("memory_writes", []))
        rows.append((match["klass"], int(match["offset"], 16), run_count, written,
                     fnv64(text)))

    rows.sort()
    with arguments.output.open("w", encoding="utf-8") as stream:
        stream.write(HEADER)
        for klass, offset, run_count, written, digest in rows:
            stream.write(f"{klass}\t0x{offset:x}\t{run_count}\t{written}\t{digest:016x}\n")
    print(f"cases={len(rows)} classes={len(set(row[0] for row in rows))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
