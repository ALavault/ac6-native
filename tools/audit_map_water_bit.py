#!/usr/bin/env python3
"""Test what CMapManager's per-position bit means, against the heightfield.

Cycles 1441-1444 established the MCA/MCI/MCD chain completely and refused four
times to name the bit, because a point query returning one bit per 8 x 8 world
units could be collision, water, no-fly or ground-vs-air, and three readings in
a row had already been wrong.

What was missing was a control. There is one now: cycle 1445 read the terrain
heightfield out of `0x82102568` -- a DIFFERENT pair of fields (`[this+0x0C]`,
`[this+0x10]`), a DIFFERENT pair of blobs (`004`, `005`) and a DIFFERENT
function from the bit query at `0x82101EE8`. Two independent structures over one
grid can be compared, and a wrong reading of either shows up as noise.

The comparison is deliberately crude on the height side: land is `h > 0.5`. That
proxy is expected to fail where ground is flat at zero -- a city -- and where
water is narrower than the 128-unit height lattice -- a river. Where it fails
matters more than how often, so this reports the residual's SHAPE, not just its
size, and renders it.

exit 0 always; this is a measurement, not a gate.

usage: audit_map_water_bit.py DIR [--ppm OUT.ppm] [--tsv OUT.tsv]
"""

import struct
import sys

MCA, MCI, MCD = "001_MCA_00.bin", "003_MCI_00.bin", "002_MCD_00.bin"
GRID, PATCHES = "004_00_01_02_03.bin", "005_Bl_02_b8.bin"
HEADER = 16
SIDE = 65
PATCH_BYTES = 0x4204
N = 1024                       # samples, 128 world units apart
WORLD_BIAS = 65536
SEA_LEVEL = 0.5


def load(d):
    r = lambda n: open("%s/%s" % (d, n), "rb").read()
    return (r(MCA)[HEADER:], r(MCI)[HEADER:], r(MCD)[HEADER:], r(GRID),
            [struct.unpack(">%df" % (SIDE * SIDE),
                           r(PATCHES)[i * PATCH_BYTES:(i + 1) * PATCH_BYTES])
             for i in range(len(r(PATCHES)) // PATCH_BYTES)])


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 0
    d = argv[1]
    ppm = tsv = None
    i = 2
    while i < len(argv):
        if argv[i] == "--ppm":
            ppm = argv[i + 1]
        elif argv[i] == "--tsv":
            tsv = argv[i + 1]
        i += 2
    mca, mci, mcd, grid, patches = load(d)

    def height(sx, sz):
        cx, rx = divmod(sx, 64)
        cz, rz = divmod(sz, 64)
        return patches[grid[cz * 16 + cx]][rz * SIDE + rx]

    def bit(sx, sz):
        cx, cz = sx >> 2, sz >> 2                       # 4 samples per cell
        grp = mca[(cz >> 4) * 16 + (cx >> 4)]
        blk = struct.unpack_from(">H", mci,
                                 (grp * 256 + (cz & 15) * 16 + (cx & 15)) * 2)[0]
        # THE RAW COORDINATES, truncated toward zero. f10/f11 are loaded once at
        # 0x82101EF4/F8 and never rewritten, and 0x821020EC multiplies those
        # untouched registers by the 0.125 at 0x8200322C -- so the +65536 that
        # forms the cell never reaches the bit index. Biasing first and flooring
        # is a DIFFERENT index for negative non-integral positions.
        #
        # On this lattice the two are provably identical: the world coordinate
        # is `s * 128 - 65536`, so `world / 8` is the exact integer `s * 16 -
        # 8192` and 8192 is a multiple of 64. Off the lattice the bit differs at
        # 0.02% of probes. Cycle 1450 found this while porting and it moved
        # nothing here, which is why the number below is unchanged.
        wx = sx * 128 - WORLD_BIAS
        wz = sz * 128 - WORLD_BIAS
        lin = ((int(wz / 8.0) & 63) << 6) | (int(wx / 8.0) & 63)
        return (mcd[blk * 512 + (lin >> 3)] >> (7 - (lin & 7))) & 1

    land = [[1 if height(x, z) > SEA_LEVEL else 0 for x in range(N)]
            for z in range(N)]
    bits = [[bit(x, z) for x in range(N)] for z in range(N)]

    tab = {}
    for z in range(N):
        for x in range(N):
            k = (land[z][x], bits[z][x])
            tab[k] = tab.get(k, 0) + 1
    tot = N * N
    for k in sorted(tab):
        print("  land=%d bit=%d : %8d (%5.2f%%)" % (k[0], k[1], tab[k],
                                                    100.0 * tab[k] / tot))
    agree = tab.get((0, 1), 0) + tab.get((1, 0), 0)
    print("bit set on water, clear on land: %.2f%% of %d samples"
          % (100.0 * agree / tot, tot))

    # WHERE the residual is. A coastline-quantisation residual hugs the
    # boundary; a wrong reading does not.
    coast = interior = 0
    for z in range(1, N - 1):
        for x in range(1, N - 1):
            if bits[z][x] != land[z][x]:
                continue                                # agrees
            me = land[z][x]
            if any(land[z + dz][x + dx] != me
                   for dz in (-1, 0, 1) for dx in (-1, 0, 1)):
                coast += 1
            else:
                interior += 1
    print("residual: %d at a land/sea boundary, %d interior (%.2f%% of all)"
          % (coast, interior, 100.0 * interior / tot))

    # Interior residual by height: a flat city sits at exactly zero.
    zero = sum(1 for z in range(N) for x in range(N)
               if bits[z][x] == land[z][x] and height(x, z) <= SEA_LEVEL)
    print("of the residual, %d samples are at or below %.1f (flat ground)"
          % (zero, SEA_LEVEL))

    if tsv:
        with open(tsv, "w") as fh:
            fh.write("land\tbit\tsamples\n")
            for k in sorted(tab):
                fh.write("%d\t%d\t%d\n" % (k[0], k[1], tab[k]))
            fh.write("#\tagreement\t%.4f\n" % (100.0 * agree / tot))
            fh.write("#\tresidual_coast\t%d\n" % coast)
            fh.write("#\tresidual_interior\t%d\n" % interior)
        print("wrote %s" % tsv)
    if ppm:
        out = bytearray()
        for z in range(N):
            for x in range(N):
                bit_value, land_value = bits[z][x], land[z][x]
                if bit_value and not land_value:
                    out += bytes((30, 60, 120))     # water bit over sea
                elif not bit_value and land_value:
                    out += bytes((90, 130, 70))     # clear bit over land
                elif bit_value and land_value:
                    out += bytes((230, 60, 60))     # bit over high ground
                else:
                    out += bytes((250, 210, 80))    # no bit over flat ground
        with open(ppm, "wb") as fh:
            fh.write(b"P6\n%d %d\n255\n" % (N, N))
            fh.write(bytes(out))
        print("wrote %s" % ppm)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
