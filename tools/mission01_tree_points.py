#!/usr/bin/env python3
"""Place the 35,846 tree points, and decide their scale by two controls.

Cycle 1490 verified 012/013 as per-cell point sets and an exclusion mask, read
by 0x82108440. The open question is the WORLD TRANSFORM: the points are
{s16 x, s16 y} in -2048..2045, which spans 4,096 units against a coarse cell of
8,192 -- so either the points occupy only the middle half of every cell
(scale x1), or one of their units is two world units (scale x2). From inside a
single cell the two are indistinguishable, because (x+2048)/32 and
(2x+4096)/64 are the same nibble index; only the world placement can tell.

TWO CONTROLS, each with a null:
  - the water bit (0x82101EE8): trees do not grow in the bay;
  - CONTINUITY at cell borders: at the wrong scale every cell's points shrink
    toward its centre and the map shows a 16 x 16 grid of gaps, which the eye
    and a border-occupancy count both catch.

usage: mission01_tree_points.py MAPDIR [--ppm OUT.ppm]
exit 0 always; this is a measurement.
"""
import struct
import sys

CELL = 8192
BIAS = 65536


def load(d):
    r = lambda n: open("%s/%s" % (d, n), "rb").read()
    return (r("012_00_01_83_b0.bin"), r("001_MCA_00.bin")[16:],
            r("003_MCI_00.bin")[16:], r("002_MCD_00.bin")[16:],
            r("004_00_01_02_03.bin"), r("005_Bl_02_b8.bin"))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 0
    d = argv[1]
    ppm = argv[argv.index("--ppm") + 1] if "--ppm" in argv else None
    wpd, mca, mci, mcd, grid, raw = load(d)
    patches = [struct.unpack(">4225f", raw[k * 16900:(k + 1) * 16900])
               for k in range(len(raw) // 16900)]

    def water(wx, wz):
        cx, cz = int((wx + BIAS) // 512), int((wz + BIAS) // 512)
        if not (0 <= cx < 256 and 0 <= cz < 256):
            return None
        grp = mca[(cz >> 4) * 16 + (cx >> 4)]
        blk = struct.unpack_from(">H", mci, (grp * 256 + (cz & 15) * 16 + (cx & 15)) * 2)[0]
        lin = ((int(wz / 8.0) & 63) << 6) | (int(wx / 8.0) & 63)
        return (mcd[blk * 512 + (lin >> 3)] >> (7 - (lin & 7))) & 1

    table = struct.unpack(">256I", wpd[:0x400])
    cells = []
    for cell in range(256):
        base = table[cell]
        hdr = struct.unpack_from(">32H", wpd, base)
        pts = []
        for j in range(16):
            off, count = hdr[j * 2], hdr[j * 2 + 1]
            for r in range(count):
                pts.append(struct.unpack_from(">2h", wpd, base + off + r * 4))
        cells.append(pts)

    results = {}
    for scale in (1, 2):
        wet = seen = 0
        border = inner = 0
        world = []
        for cell, pts in enumerate(cells):
            cx, cz = cell % 16, cell // 16
            ox = cx * CELL - BIAS + CELL // 2
            oz = cz * CELL - BIAS + CELL // 2
            for x, y in pts:
                wx, wz = ox + x * scale, oz + y * scale
                world.append((wx, wz))
                w = water(wx, wz)
                if w is None:
                    continue
                seen += 1
                wet += w
                # within 512 units of a cell border?
                lx = abs(x * scale)
                if lx > CELL // 2 - 512 or abs(y * scale) > CELL // 2 - 512:
                    border += 1
                else:
                    inner += 1
        results[scale] = (wet, seen, border, inner, world)
        print("scale x%d: %d points, %.2f%% on water, border-band occupancy "
              "%.1f%% (a uniform spread would give ~%.0f%%)"
              % (scale, seen, 100.0 * wet / seen, 100.0 * border / (border + inner),
                 100.0 * (1 - ((CELL - 1024.0) / CELL) ** 2)))

    if ppm:
        # land/water backdrop at 512 units per pixel, both scales side by side
        N = 256
        img = bytearray()
        for half in range(2):
            pass
        rows = []
        for scale in (1, 2):
            pix = [[(30, 52, 86) if water(x * 512 - BIAS + 256, z * 512 - BIAS + 256)
                    else (70, 96, 60) for x in range(N)] for z in range(N)]
            for wx, wz in results[scale][4]:
                px, pz = int((wx + BIAS) // 512), int((wz + BIAS) // 512)
                if 0 <= px < N and 0 <= pz < N:
                    pix[pz][px] = (240, 220, 90)
            rows.append(pix)
        with open(ppm, "wb") as fh:
            fh.write(b"P6\n%d %d\n255\n" % (N * 2 + 8, N))
            for z in range(N):
                line = bytearray()
                for x in range(N):
                    line += bytes(rows[0][z][x])
                line += bytes((0, 0, 0)) * 8
                for x in range(N):
                    line += bytes(rows[1][z][x])
                fh.write(bytes(line))
        print("wrote %s  (left: x1, right: x2)" % ppm)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
