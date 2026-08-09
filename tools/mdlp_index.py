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

    python3 tools/mdlp_index.py PATH/TO/001_MDLP.mdlp [--exceptions]
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


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
