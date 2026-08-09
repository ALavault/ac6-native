#!/usr/bin/env python3
"""Place the map parts, and test the placement against two independent grids.

Since cycle 1440 the open question has been the same one: the 178 map parts are
in local coordinates, centred on their own origins, and *something else* places
them. `011_00_00_00_00.bin` is that something.

    header   256 records of 16 bytes, one per coarse cell, cz * 16 + cx
             { u32 count; u32 offset; u32 ?; u32 zero }
    body     `count` instances at `offset`, 16 bytes each
             { float x; float y; float z; u32 tag }

The header is a PARTITION, not an index: its 256 counts sum to exactly 4318,
which is exactly `(file - 4096) / 16`. Nothing was fitted to make that hold.

Positions are local to the cell and run to +/-4096, and a coarse cell is 8192
world units (16 cells of 512, cycle 1442), so the local origin is the cell's
CENTRE and the world position is

    world = cell_index * 8192 - 65536 + 4096 + local

THE CONTROLS, and there are two, each against a structure derived from a
different blob by a different retail function:

  - the water bit of `0x82101EE8` (MCA/MCI/MCD). Buildings should be on land.
  - the heightfield of `0x82102568` (`004`/`005`). A city should sit on the flat
    ground at elevation zero, which is exactly where cycle 1445's water-bit
    residual was concentrated.

Both are reported with a null model, because "99% on land" means nothing until
you know what a random scatter scores.

`tag & 0xFFFF` is a part id in 0..172 against 178 parts. `tag >> 16` varies over
171 values and is NOT named here -- a rotation is the obvious reading and the
obvious reading is what cycles 1428, 1440 and 1441 each got wrong.

usage: mission01_map_instances.py DIR [--ppm OUT.ppm] [--tsv OUT.tsv]
exit 0 always; this is a measurement, not a gate.
"""

import math
import random
import struct
import sys

CELL_UNITS = 512
COARSE_UNITS = CELL_UNITS * 16          # 8192
WORLD_BIAS = 65536
HEADER_RECORDS = 256
RECORD_BYTES = 16


def load(d):
    r = lambda n: open("%s/%s" % (d, n), "rb").read()
    return (r("011_00_00_00_00.bin"), r("001_MCA_00.bin")[16:], r("003_MCI_00.bin")[16:],
            r("002_MCD_00.bin")[16:], r("004_00_01_02_03.bin"), r("005_Bl_02_b8.bin"))


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
    blob, mca, mci, mcd, grid, raw = load(d)
    patches = [struct.unpack(">4225f", raw[k * 16900:(k + 1) * 16900])
               for k in range(len(raw) // 16900)]

    def water(wx, wz):
        cx, cz = int((wx + WORLD_BIAS) // CELL_UNITS), int((wz + WORLD_BIAS) // CELL_UNITS)
        if not (0 <= cx < 256 and 0 <= cz < 256):
            return None
        grp = mca[(cz >> 4) * 16 + (cx >> 4)]
        blk = struct.unpack_from(">H", mci, (grp * 256 + (cz & 15) * 16 + (cx & 15)) * 2)[0]
        lin = ((int((wz + WORLD_BIAS) // 8) & 63) << 6) | (int((wx + WORLD_BIAS) // 8) & 63)
        return (mcd[blk * 512 + (lin >> 3)] >> (7 - (lin & 7))) & 1

    def height(wx, wz):
        sx, sz = int((wx + WORLD_BIAS) // 128), int((wz + WORLD_BIAS) // 128)
        if not (0 <= sx <= 1024 and 0 <= sz <= 1024):
            return None
        cx, rx = divmod(sx, 64)
        cz, rz = divmod(sz, 64)
        if cx == 16: cx, rx = 15, 64
        if cz == 16: cz, rz = 15, 64
        return patches[grid[cz * 16 + cx]][rz * 65 + rx]

    # THE PARTITION. Read it and check it before using it.
    total = 0
    header = []
    for k in range(HEADER_RECORDS):
        count, offset, third, fourth = struct.unpack_from(">4I", blob, k * RECORD_BYTES)
        header.append((count, offset, third, fourth))
        total += count
    body = (len(blob) - HEADER_RECORDS * RECORD_BYTES) // RECORD_BYTES
    print("header counts sum to %d; the body holds %d records -- %s"
          % (total, body, "a partition" if total == body else "NOT a partition"))
    if total != body:
        return 0

    instances = []
    for k, (count, offset, _third, _fourth) in enumerate(header):
        if count == 0:
            continue
        cx, cz = k % 16, k // 16
        for j in range(count):
            o = offset + j * RECORD_BYTES
            lx, ly, lz = struct.unpack_from(">3f", blob, o)
            tag = struct.unpack_from(">I", blob, o + 12)[0]
            instances.append((cx * COARSE_UNITS - WORLD_BIAS + COARSE_UNITS // 2 + lx,
                              ly,
                              cz * COARSE_UNITS - WORLD_BIAS + COARSE_UNITS // 2 + lz,
                              tag & 0xFFFF, tag >> 16))
    print("placed %d instances, %d distinct part ids (0..%d)"
          % (len(instances), len({q[3] for q in instances}),
             max(q[3] for q in instances)))
    xs = [q[0] for q in instances]
    zs = [q[2] for q in instances]
    print("world x %.0f..%.0f  z %.0f..%.0f" % (min(xs), max(xs), min(zs), max(zs)))

    on_land = sum(1 for q in instances if water(q[0], q[2]) == 0)
    random.seed(1)
    null = sum(1 for _ in instances
               if water(random.uniform(-65000, 65000), random.uniform(-65000, 65000)) == 0)
    print("on land: %.1f%%   CONTROL, a random scatter: %.1f%%"
          % (100.0 * on_land / len(instances), 100.0 * null / len(instances)))

    ground = [height(q[0], q[2]) for q in instances]
    ground = [g for g in ground if g is not None]
    flat = sum(1 for g in ground if g < 1.0)
    random.seed(2)
    null_g = [height(random.uniform(-65000, 65000), random.uniform(-65000, 65000))
              for _ in instances]
    null_g = [g for g in null_g if g is not None]
    null_flat = sum(1 for g in null_g if g < 1.0)
    print("ground under them below 1.0: %.1f%%   CONTROL: %.1f%%"
          % (100.0 * flat / len(ground), 100.0 * null_flat / len(null_g)))
    print("their y: %d of %d exactly zero, range %.1f..%.1f"
          % (sum(1 for q in instances if q[1] == 0.0), len(instances),
             min(q[1] for q in instances), max(q[1] for q in instances)))

    if tsv:
        with open(tsv, "w") as fh:
            fh.write("world_x\tworld_y\tworld_z\tpart_id\ttag_high\tground\n")
            for q in instances:
                g = height(q[0], q[2])
                fh.write("%.3f\t%.3f\t%.3f\t%d\t%d\t%s\n"
                         % (q[0], q[1], q[2], q[3], q[4],
                            "%.3f" % g if g is not None else ""))
        print("wrote %s" % tsv)

    if ppm:
        N = 1025
        out = bytearray()
        for sz in range(N):
            for sx in range(N):
                h = height(sx * 128 - WORLD_BIAS, sz * 128 - WORLD_BIAS)
                if h is None or not math.isfinite(h):
                    out += bytes((20, 20, 24))
                elif h <= 0.5:
                    out += bytes((30, 52, 86))
                else:
                    v = min(255, 70 + int(h * 0.35))
                    out += bytes((v // 2, v, v // 2))
        for q in instances:
            sx = int((q[0] + WORLD_BIAS) / 128.0)
            sz = int((q[2] + WORLD_BIAS) / 128.0)
            for dz in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    x, z = sx + dx, sz + dz
                    if 0 <= x < N and 0 <= z < N:
                        o = (z * N + x) * 3
                        out[o] = 250; out[o + 1] = 170; out[o + 2] = 40
        with open(ppm, "wb") as fh:
            fh.write(b"P6\n%d %d\n255\n" % (N, N))
            fh.write(bytes(out))
        print("wrote %s" % ppm)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
