#!/usr/bin/env python3
"""Read 001_MDLP.mdlp's entry table -- the integer-indexed array Thread B needs.

WHY THIS EXISTS. The plan's first Thread B decision is how a model is loaded.
Cycle 1246 established that retail resolves assets BY INTEGER ID through
registries and never walks the directory, so porting an FHM directory walk would
be porting something the game does not do, and an offline extraction is a
manifest under another name.

Cycle 1418 found that the question largely answers itself, because the file
format IS an array:

    +0x00  "MDLP"
    +0x04  entry count            94 for Mission 01
    +0x08  the file's byte length exactly
    +0x0C  offset of the entry table   0x1000
    +0x10  offset of the data          0x2000

and entry `i` sits at `data + table[i]`. All 94 resolve to an `FHM ` magic and
the offsets ascend. `MDLP[id]` is an array index; there is no walk to port.

WHAT IS NOT ESTABLISHED, and a plausible rule died here. The NDXR-bearing
entries looked strictly even -- 0, 2, 4, 6, 8, ... -- which reads as 47 pairs of
{geometry, textures}. Checking the exceptions rather than the pattern:
**45 of the 94 entries break it.** Most even entries also carry an NTXR, four
odd entries carry NDXR, and entries 87 and 89 are empty 4096-byte stubs. The
pairing is NOT established and this tool does not claim it.

CYCLE 1419 ADDED THE SECOND LEVEL, and it is the same shape again:

    +0x00  "FHM "
    +0x10  sub-entry count
    +0x14  `count` big-endian offsets, relative to the FHM's own base

So the whole resolution is nested arrays and nothing has to be searched for:

    MDLP[i] -> FHM -> FHM[j] -> {NDXR | NTXR | MATE | a nested FHM}

Checked across the whole file: 94 of 94 FHMs parse, 1,480 sub-entries, every
table ascending, no anomalies -- and **292 of 292 NDXR occurrences sit at a
tabulated offset**, so the byte-scan cycle 1418 used is a search this replaces
with a lookup.

THE TWO LENGTH SOURCES DO NOT DISAGREE, though the first comparison said they
did. The FHM table implies a span; the container declares its own length at
+0x04; the table's span is the larger in all 292 cases and **every byte of the
difference is zero**. One is a padded span and the other a content length, which
is not a contradiction -- reporting "292 disagreeing" would have been.

    python3 tools/mdlp_index.py PATH/TO/001_MDLP.mdlp [--exceptions] [--resolve]
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


def read_fhm(data: bytes, base: int, limit: int) -> list[dict]:
    """The sub-entries of the FHM at `base`, by its own table at +0x14."""
    be = lambda o: struct.unpack_from(">I", data, o)[0]  # noqa: E731
    if data[base:base + 4] != b"FHM ":
        raise ValueError("not an FHM at %#x: %r" % (base, data[base:base + 4]))
    count = be(base + 0x10)
    if count == 0 or base + 0x14 + 4 * count > limit:
        raise ValueError("FHM at %#x declares %d sub-entries, which does not fit"
                         % (base, count))
    offsets = [be(base + 0x14 + 4 * i) for i in range(count)]
    out = []
    for i, offset in enumerate(offsets):
        start = base + offset
        end = base + offsets[i + 1] if i + 1 < count else limit
        out.append({"index": i, "offset": start, "span": end - start,
                    "magic": bytes(data[start:start + 4])})
    return out


def read_index(data: bytes) -> dict:
    be = lambda o: struct.unpack_from(">I", data, o)[0]  # noqa: E731
    if data[:4] != b"MDLP":
        raise ValueError("not an MDLP: magic is %r" % data[:4])
    count, declared, table, body = be(4), be(8), be(0xC), be(0x10)
    offsets = [be(table + 4 * i) for i in range(count)]
    entries = []
    for index, offset in enumerate(offsets):
        start = body + offset
        end = body + offsets[index + 1] if index + 1 < count else len(data)
        blob = data[start:end]
        entries.append({
            "index": index, "offset": start, "size": end - start,
            "magic": blob[:4].decode("latin1"),
            "ndxr": blob.count(b"NDXR"), "ntxr": blob.count(b"NTXR"),
        })
    return {"count": count, "declared_size": declared, "actual_size": len(data),
            "table": table, "body": body, "entries": entries}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--exceptions", action="store_true",
                        help="list the entries that break the even/odd reading")
    parser.add_argument("--resolve", action="store_true",
                        help="walk MDLP[i] -> FHM[j] and report every container "
                             "reached by INDEX rather than by scanning for a magic")
    args = parser.parse_args()
    index = read_index(args.path.read_bytes())

    print(f"count={index['count']}  declared_size={index['declared_size']}  "
          f"actual={index['actual_size']}  "
          f"match={index['declared_size'] == index['actual_size']}")
    print(f"table={index['table']:#x}  body={index['body']:#x}")
    magics = {}
    for entry in index["entries"]:
        magics[entry["magic"]] = magics.get(entry["magic"], 0) + 1
    print("entry magics: " + ", ".join(f"{k!r}x{v}" for k, v in magics.items()))
    print(f"NDXR total {sum(e['ndxr'] for e in index['entries'])}, "
          f"NTXR total {sum(e['ntxr'] for e in index['entries'])}")

    if args.resolve:
        data = args.path.read_bytes()
        be = lambda o: struct.unpack_from(">I", data, o)[0]  # noqa: E731
        kinds, ndxr, padded = {}, 0, 0
        for entry in index["entries"]:
            subs = read_fhm(data, entry["offset"], entry["offset"] + entry["size"])
            for sub in subs:
                kinds[sub["magic"]] = kinds.get(sub["magic"], 0) + 1
                if sub["magic"] == b"NDXR":
                    ndxr += 1
                    declared = be(sub["offset"] + 4)
                    tail = data[sub["offset"] + declared:sub["offset"] + sub["span"]]
                    if declared <= sub["span"] and not any(tail):
                        padded += 1
        print(f"\nresolved by index: {sum(kinds.values())} sub-entries")
        for magic, n in sorted(kinds.items(), key=lambda kv: -kv[1])[:8]:
            print(f"   {magic!r:10} {n}")
        print(f"NDXR reached by index: {ndxr}; "
              f"declared length fits its span with zero padding: {padded}")

    if args.exceptions:
        broken = [e for e in index["entries"]
                  if (e["index"] % 2 == 0 and (e["ndxr"] == 0 or e["ntxr"] > 0))
                  or (e["index"] % 2 == 1 and (e["ndxr"] > 0 or e["ntxr"] == 0))]
        print(f"\nentries breaking 'even=geometry, odd=textures': "
              f"{len(broken)} of {index['count']}")
        for entry in broken:
            print(f"  {entry['index']:>3}  NDXR={entry['ndxr']:<3} "
                  f"NTXR={entry['ntxr']:<3} bytes={entry['size']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
