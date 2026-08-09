# Cycle 1491 — one unit is two

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the ported grids.
- No product C++ changed; ctest stays **60**. **No contract entry.**
- New: `tools/mission01_tree_points.py`.
- Lands before cycle 1489's pending video commit, like 1490.

## The question the tree points carried

Their locals are `{s16, s16}` in **−2048..2045** — a 4,096-unit span against a
coarse cell of 8,192. From inside one cell the two readings are
indistinguishable, because `(x+2048)/32` and `(2x+4096)/64` are the same nibble
index; cycle 1490's 99.1% mask correlation could not tell them apart. Only the
world placement can.

## Two controls, and both decide the same way

```
scale x1: 1.91% on water   border-band occupancy  0.1%  (uniform predicts ~23%)
scale x2: 0.05% on water   border-band occupancy 23.1%  (uniform predicts ~23%)
```

- **Continuity**: at x2 the border band holds exactly the share a uniform
  spread predicts; at x1 it holds nothing, and the overlay shows a 16 x 16
  lattice of shrunken squares — vegetation with cell-shaped moats, which is not
  a thing.
- **Water**: at x2, 110 points of 214,009 touch the bay; at x1, 38 times more.

> **One point unit is two world units.** `world = cell*8192 − 65536 + 4096 +
> local*2`, and the point domain covers the full cell.

`mission01-tree-scale.png` shows both side by side; the x2 forest stops at the
bay and thins at the city, which nothing in the transform was told about.

## What this settles beyond the trees

The sweep's synthesis flagged the map's world scale as a live contradiction.
This is an independent vote for the established reading: the coarse cell is
**8,192 world units** (16 cells of 512), because a 4,096-unit cell would have
made scale x1 the continuous one. The heightfield, the water bit, the placement
list and now the tree points all agree on one metric.

The 214,014 placed points against 35,846 unique records is block sharing — the
256 cells reference 24 blocks, the same reuse MCD showed at cycle 1443.

## Not established

- The vertical: points carry no height and the trees presumably sit on the
  heightfield; unverified.
- Kind selection per point (the mask nibble at the point's own position) and
  the Kind/SetNo → model mapping, unchanged from 1490.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**Cycle 1489's video, the moment the render completes** — then the trees go
into the sequence as billboards at the verified transform, which is now a
one-liner instead of a guess.
