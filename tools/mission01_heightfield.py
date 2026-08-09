#!/usr/bin/env python3
"""Assemble and render the Mission 01 terrain heightfield.

The layout is read out of `0x82102568` (CMapManager's segment-vs-terrain test),
not guessed:

    r6 = [this+0x0C]                     a 16 x 16 byte grid of patch ids
    r8 = [this+0x10]                     the patch array
    coarse = (cell >> 4), 0..15 both axes, bounds-checked
    patch  = r6[coarse_z * 16 + coarse_x]          (lbzx at 0x8210263C)
    base   = patch * 0x4204 + r8                   (mulli at 0x82102648)
    off    = 4 * (260 * (cell_z & 15) + 4 * (cell_x & 15) + k)
    row step in the sampler loop is 0x104 = 260 bytes             (0x821027C0)

260 bytes is 65 floats and 65 * 260 = 16900 = 0x4204, so a patch is 65 x 65
floats: 16 cells of 4 samples, plus the shared edge. One cell is 512 world
units (cycle 1442), so a sample is **128 world units**, and the 16 x 16 x 64 + 1
grid spans the same +/-65536 the query's `+65536.0` implies.

In the Mission 01 archive that is `004_00_01_02_03.bin` (74 distinct ids) and
`005_Bl_02_b8.bin` (74 * 16900 + 4 bytes).

THE CONTROL, and it is the reason this is a reading rather than a proposal: if
the 65th row and column are the shared edge, a patch's column 64 must equal its
right neighbour's column 0. It does, for all 15600 samples along each axis, to
0.0000 -- while random block pairs compared the same way mismatch 9264 of 13000.

usage: mission01_heightfield.py DIR [--png OUT.ppm] [--tsv OUT.tsv]
       mission01_heightfield.py .../021_FHM --png relief.ppm
exit 0 on success, 2 if a control fails.
"""

import math
import os
import struct
import sys

GRID = "004_00_01_02_03.bin"
PATCHES = "005_Bl_02_b8.bin"
PATCH_BYTES = 0x4204          # mulli r10,r10,0x4204 at 0x82102648
ROW_BYTES = 0x104             # addi  r10,r10,0x104  at 0x821027C0
SIDE = ROW_BYTES // 4         # 65 floats per row
CELL_UNITS = 512.0            # 1/0.001953125, cycle 1442
SAMPLE_UNITS = CELL_UNITS / 4.0
WORLD_BIAS = 65536.0          # 0x82069BB8


def load(directory):
    grid = open(os.path.join(directory, GRID), "rb").read()
    raw = open(os.path.join(directory, PATCHES), "rb").read()
    n = len(raw) // PATCH_BYTES
    patches = [struct.unpack(">%df" % (SIDE * SIDE),
                             raw[i * PATCH_BYTES:(i + 1) * PATCH_BYTES])
               for i in range(n)]
    return grid, patches, len(raw) - n * PATCH_BYTES


def assemble(grid, patches):
    """The 1025 x 1025 sample field, [z][x]; None where a patch is absent."""
    side = 16 * (SIDE - 1) + 1
    field = [[None] * side for _ in range(side)]
    for cz in range(16):
        for cx in range(16):
            pid = grid[cz * 16 + cx]
            if pid >= len(patches):
                continue
            p = patches[pid]
            for r in range(SIDE):
                base = r * SIDE
                row = field[cz * (SIDE - 1) + r]
                for c in range(SIDE):
                    row[cx * (SIDE - 1) + c] = p[base + c]
    return field


def check_edges(grid, patches):
    ok = bad = 0
    for cz in range(16):
        for cx in range(16):
            a = patches[grid[cz * 16 + cx]]
            if cx < 15:
                b = patches[grid[cz * 16 + cx + 1]]
                for r in range(SIDE):
                    ok, bad = _tally(a[r * SIDE + SIDE - 1], b[r * SIDE], ok, bad)
            if cz < 15:
                b = patches[grid[(cz + 1) * 16 + cx]]
                for c in range(SIDE):
                    ok, bad = _tally(a[(SIDE - 1) * SIDE + c], b[c], ok, bad)
    return ok, bad


def _tally(u, v, ok, bad):
    if not (math.isfinite(u) and math.isfinite(v)):
        return ok, bad
    return (ok + 1, bad) if abs(u - v) <= 1e-3 else (ok, bad + 1)


RAMP = [(0, (24, 48, 84)), (1, (36, 84, 120)), (2, (196, 186, 140)),
        (60, (74, 118, 62)), (160, (120, 128, 66)), (300, (146, 122, 88)),
        (420, (188, 184, 180)), (520, (250, 250, 250))]


def colour(h):
    for i in range(len(RAMP) - 1):
        a, ca = RAMP[i]
        b, cb = RAMP[i + 1]
        if h <= b:
            t = 0.0 if b == a else (h - a) / (b - a)
            return tuple(int(ca[k] + (cb[k] - ca[k]) * t) for k in range(3))
    return RAMP[-1][1]


def render(field, path):
    side = len(field)
    out = bytearray()
    for z in range(side):
        for x in range(side):
            h = field[z][x]
            if h is None or not math.isfinite(h):
                out += bytes((20, 20, 24))
                continue
            # hillshade from the west, one sample = SAMPLE_UNITS across
            hx = field[z][min(x + 1, side - 1)]
            hz = field[min(z + 1, side - 1)][x]
            dx = (hx - h) if (hx is not None and math.isfinite(hx)) else 0.0
            dz = (hz - h) if (hz is not None and math.isfinite(hz)) else 0.0
            nx, nz = -dx / SAMPLE_UNITS, -dz / SAMPLE_UNITS
            inv = 1.0 / math.sqrt(nx * nx + nz * nz + 1.0)
            shade = max(0.25, min(1.35, (0.55 * nx + 0.35 * nz + 0.78) * inv + 0.30))
            r, g, b = colour(h)
            out += bytes((min(255, int(r * shade)), min(255, int(g * shade)),
                          min(255, int(b * shade))))
    with open(path, "wb") as fh:
        fh.write(b"P6\n%d %d\n255\n" % (side, side))
        fh.write(bytes(out))
    return side


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 0
    directory = argv[1]
    png = tsv = None
    i = 2
    while i < len(argv):
        if argv[i] == "--png":
            png = argv[i + 1]
        elif argv[i] == "--tsv":
            tsv = argv[i + 1]
        i += 2

    grid, patches, slack = load(directory)
    print("patch grid   %s: %d bytes, %d distinct ids, max %d"
          % (GRID, len(grid), len(set(grid)), max(grid)))
    print("patch array  %s: %d patches of %d bytes, %d byte(s) over"
          % (PATCHES, len(patches), PATCH_BYTES, slack))

    ok, bad = check_edges(grid, patches)
    print("shared-edge control: %d match, %d mismatch" % (ok, bad))
    if bad:
        print("CONTROL FAILED -- the 65th row/column is not the shared edge")
        return 2

    field = assemble(grid, patches)
    vals = [h for row in field for h in row if h is not None and math.isfinite(h)]
    side = len(field)
    print("field %d x %d samples, %.1f units apart, %.0f units across"
          % (side, side, SAMPLE_UNITS, (side - 1) * SAMPLE_UNITS))
    print("height %.2f .. %.2f over %d samples" % (min(vals), max(vals), len(vals)))
    print("world x,z span %.0f .. %.0f"
          % (-WORLD_BIAS, -WORLD_BIAS + (side - 1) * SAMPLE_UNITS))

    if tsv:
        with open(tsv, "w") as fh:
            fh.write("patch_id\tmin\tmax\tmean\tflat\n")
            for pid, p in enumerate(patches):
                f = [x for x in p if math.isfinite(x)]
                flat = 1 if len(set(f)) <= 1 else 0
                fh.write("%d\t%.4f\t%.4f\t%.4f\t%d\n"
                         % (pid, min(f), max(f), sum(f) / len(f), flat))
        print("wrote %s" % tsv)
    if png:
        render(field, png)
        print("wrote %s" % png)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
