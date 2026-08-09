# Cycle 1443 — three levels

## Qualification

- **No Ghidra run and no oracle pass.** The image, the corpus and the blobs.
- No product C++ changed; ctest stays **53**. **No contract entry** — nothing is
  ported yet.

## The whole structure

`0x82101EE8` reads end to end as a **three-level sparse grid**, and every level's
arithmetic is confirmed by the blobs' own sizes:

```
world (x at +0, z at +8; the vertical is skipped)
  cell = (world + 65536) * 1/512                      256 x 256 cells of 512 units

MCA   [ (cz>>4 + 1)*16 + (cx>>4) ]        -> group      a 16 x 16 byte grid
        bounds-checked against [MCI+8]
MCI   [ group*256 + (cz&15)*16 + (cx&15) ] -> block     u16, from MCI+16
        bounds-checked against [MCD+8]
MCD    block*512 + 16                      -> 512 bytes = 64 x 64 bits

  bit = ((world * 1/8) & 63) for each axis           8 world units per bit
  return (byte >> (7 - (index & 7))) & 1
```

## Every count matches

| | the code checks | the file holds |
|---|---|---|
| MCA values | `< [MCI+8]` = **4864** | 0…**18**, 19 distinct |
| MCI entries | — | 9,728 bytes = **4,864** u16 = **19 × 256** |
| MCI values | `< [MCD+8]` = **413** | — |
| MCD blocks | `block × 512` | 211,456 = **413 × 512** |

**19 groups × 256 sub-cells is exactly MCI's entry count, and 19 is exactly the
number of distinct MCA values.** Neither was fitted: MCA's value set was read at
cycle 1440, MCI's size at 1441, and the multipliers out of the instructions this
cycle.

And the last constant closes it geometrically: **1/8 per bit × 64 bits = 512
units**, which is one coarse cell and the cap on every one of the 178 map parts.
`(world + 65536)` over 512 gives 256 cells per axis, and 256 × 512 = 131,072 =
the full ±65,536 span.

## What cycle 1441 drew

Cycle 1441 rendered MCD's records as 64 × 64 masks because 4,096 bits is the only
square factoring, and called that "plausible and not established". It is 64 × 64,
and the establishing is `rlwinm r11,r11,6,20,25` — six bits of row, six of
column — in the function that indexes them.

The same cycle proposed a height-stack reading and killed it. The masks are not a
stack; they are **413 distinct blocks shared by reference**, which is why they
are neither monotone nor nested and why there are 413 of them for 65,536 cells.
A sparse grid reuses its blocks.

## What is still not established

- **What the bit means.** The structure is complete and the semantics are not: a
  point query returning one bit per 8 × 8 world units could be collision, water,
  no-fly, or ground-vs-air. Nothing read says which, and the three cycles before
  this one are a good argument for not guessing.
- Which of the two functions that reference the strings is the loader; only the
  query was read.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Find the caller and the bit's meaning follows.** `0x82101EE8` has no callers in
the corpus — a vtable slot, like `0x82303110` and `0x820A8138` before it — so
`tools/whose_vtable.py` on its address is the same move that named
`CX360UnitManager` at cycle 1385 and `CX360ActorModelSetup` at 1422.

The class that owns a per-position bit query will say what the bit is for, and
that is a lookup rather than a guess.
