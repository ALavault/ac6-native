#!/usr/bin/env python3
"""Decode the map's .sph sky-sphere records.

The loader at 0x820FC900 reads /map/mapset/sph%s.sph (fallback sphdef.sph) into
an inline buffer at CMapManager+0x6850 and byte-reverses a HARD-CODED 88 words
at 0x820FC960..0x820FC998 -- so the file is exactly 352 bytes, stored
little-endian, two records of 0xB0 = 176 bytes (the +0xB0 stride is at
0x820FA53C). Mission 01's file is 022_FHM/002_00_00pF.bin.

THE RECORD, as far as the bytes show it (the consumer at CSkySphere has not
been read, so every name below is a reading of the values, not of the code):

  +0x00  f32 x2     15360 / 25600 in record 0 -- radii or near/far, unnamed
  +0x10  u8[4] x12  twelve RGB0 colours, a 3 x 4 grid          "palette A"
  +0x40  u8[4] x12  twelve more                                 "palette B"
  +0x70  u8[4] x2   zeros, then two dark RGB0 colours
  +0x80  f32 x12    0, 0, 0.57988, 0, then eight small weights

Record 0 is grey-blue and record 1 saturated blue: the same two-state split as
sky1/sky2 in the mapset XML (cycle 1482's sweep: same 127 keys, two value
sets). Within a palette the three ROWS run bright to dark; the four COLUMNS'
meaning is not established (azimuth relative to the sun is the natural guess
and is not taken).

usage: mission01_sph_dump.py SPH_FILE
"""
import struct
import sys


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 0
    data = open(argv[1], "rb").read()
    if len(data) != 352:
        print("sph_dump=refused size %d != 352 (the loader swaps exactly 88 words)"
              % len(data))
        return 1
    for record in range(2):
        base = record * 176
        head = struct.unpack("<2f", data[base:base + 8])
        print("record %d: head %.6g / %.6g" % (record, head[0], head[1]))
        for name, off in (("palette A", 0x10), ("palette B", 0x40)):
            print("  %s:" % name)
            for row in range(3):
                cells = []
                for col in range(4):
                    o = base + off + (row * 4 + col) * 4
                    cells.append("(%3d,%3d,%3d)" % tuple(data[o:o + 3]))
                print("    row %d: %s" % (row, " ".join(cells)))
        extra = [tuple(data[base + 0x78 + i:base + 0x78 + i + 3]) for i in (0, 4)]
        tail = struct.unpack("<12f", data[base + 0x80:base + 0xB0])
        print("  extra colours: %s   tail: %s"
              % (extra, " ".join("%.5g" % v for v in tail)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
