#!/usr/bin/env python3
"""The map's scene parameters, which are plain text and were never opened.

Cycle 1442 found `.mapparts.distanceL`, `.mapparts.mipBias` and their siblings
in the EXECUTABLE's string table and concluded the `_l_`/`_m_`/`_s_` suffixes on
the 178 map parts are draw-distance classes. It could not say what the distances
were, because the executable holds the names and not the values.

The values are in the archive, in readable XML. `0x820FC8E0` in the map loader
formats `/map/mapset` and `0x820FC900` formats `sph%s.sph` under it, so there is
a second container beside the map -- and in Mission 01's `idx_0119` that is
`022_FHM`, whose first entry begins `EF BB BF`, a UTF-8 byte-order mark.

It carries **416 named values** in eleven groups: `.sky1`, `.sky2`, `.HDR`,
`.LensFlare`, `.Vignetting`, `.tree`, `.mapparts`, `.player`, `.debug`, `.A`,
`.B`. Among them are the sun's two angles, the fog's far distance and density,
and the three draw distances -- every one of which this campaign's renderers had
been inventing.

usage: mission01_mapset_params.py MAPSET_FHM_DIR [--tsv OUT.tsv]
exit 0 always; this is a reader.
"""

import os
import re
import sys

ENTRY = re.compile(r'<(\w+)\s+name="([^"]+)"\s*>([^<]*)</\1>')


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 0
    directory = argv[1]
    tsv = argv[argv.index("--tsv") + 1] if "--tsv" in argv else None

    documents = []
    for name in sorted(os.listdir(directory)):
        path = os.path.join(directory, name)
        if not os.path.isfile(path):
            continue
        head = open(path, "rb").read(3)
        if head != b"\xef\xbb\xbf":            # the BOM is how they are found
            continue
        documents.append((name, open(path, encoding="utf-8-sig",
                                     errors="replace").read()))

    rows = []
    for name, text in documents:
        for kind, key, value in ENTRY.findall(text):
            rows.append((name, kind, key, value.strip()))
    print("documents %d, values %d" % (len(documents), len(rows)))

    groups = {}
    for _n, _k, key, _v in rows:
        groups.setdefault(key.split(".")[1] if key.startswith(".") else key, 0)
        groups[key.split(".")[1] if key.startswith(".") else key] += 1
    print("groups: %s" % "  ".join("%s=%d" % kv for kv in sorted(groups.items())))

    interesting = [".sky1.sun.lrx", ".sky1.sun.lry", ".sky1.fog.far",
                   ".sky1.fog.density", ".mapparts.distanceL",
                   ".mapparts.distanceM", ".mapparts.distanceS"]
    for key in interesting:
        for _n, _k, k, v in rows:
            if k == key:
                print("  %-28s %s" % (key, v))
                break

    if tsv:
        with open(tsv, "w") as handle:
            handle.write("document\ttype\tname\tvalue\n")
            for row in rows:
                handle.write("\t".join(row) + "\n")
        print("wrote %s" % tsv)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
