# Cycle 1490 — the trees were in the map

## Qualification

- **No Ghidra run and no oracle pass.** The archive, the image via
  `tools/ppc_read.py`, and the sweep's synthesis — every claim used re-measured
  here.
- No product C++ changed; ctest stays **60**. **No contract entry.**
- **Lands before cycle 1489's commit**, which is waiting on a video render;
  1489's tool edits are deliberately not in this commit.

## The sweep's cheapest cycle, taken and verified

The synthesis named it: `021_FHM/012` and `013` are `m01.wpd` and `m01.wsd` —
the tree placement files `CTreeGenerator` loads by hardcoded name (cycle 1482:
`sim:map/m01/m01.wsd` at `0x8205CBE0`, `m01.wpd` at `0x8205CBF4`), which cycle
1482 recorded as absent from the archive. They were in the map container the
whole time, two entries past the placement list.

**Re-measured, all three arithmetic claims hold:**

```
013:  256-entry u32 table, every value 0x400 + k*0x2000, k = 0..23,
      and 0x400 + 24*0x2000 = 197,632 = the file size exactly
012:  256-entry u32 table over 24 blocks; each block's 16-slot
      {u16 offset, u16 count} header closes inside its block, 24 of 24;
      35,846 records of {s16 x, s16 y}, range -2048..2045
mask: points land on nibble 0 at 99.1% against an 84.6% baseline
      (the sweep's 99.7/80.9, re-derived with per-cell multiplicity)
```

So `013` is a per-coarse-cell **128 x 128 nibble mask** (32 world units per
nibble over the 4096-unit half-cell span) and `012` is per-cell **point sets**,
and the points sit where the mask is clear — placement positions and their
exclusion mask.

## Anchored to the reader's own instructions

`0x82108440` (172 instructions by `.pdata`), `CTreeGenerator`'s slot:

```
0x821085F8  lbz    r9,0x0(r6)
0x82108600  rlwinm r9,r9,28,4,31      the HIGH nibble
0x8210864C  lbz    r9,0x0(r6)
0x82108654  rlwinm r9,r9,0,28,31      the LOW nibble
0x821086A0  cmpiw  cr6,r11,0x4000     0x4000 = 128 * 128, the grid bound
```

The nibble split and the 16,384 bound are in the code, byte for byte what the
file's structure demands. The nibble value indexes a table at `[r9+0x4C]` — the
`Kind`/`SetNo` presets the mapset's `.tree` group carries (59 values, read at
cycle 1482), which is how a nibble chooses *which* tree.

**The one link taken from the sweep and not retraced here**: which file is
`wsd` and which `wpd`. It rests on the load order at `0x8210836C` (first file →
`this+0x04`) and `0x821083B4` (second → `this+0x08`) and on which pointer this
function walks — the agent's read, with addresses, not repeated by me.

## What this retires

Cycle 1482 recorded the tree files as absent, and the vegetation investigation
concluded no tree could be drawn from what was on disk. **That negative is
dead**: positions, exclusion mask, per-nibble kind selection and the preset
table are all in hand. What a tree still needs is its *model* — the presets name
`Kind`/`SetNo`, and nothing yet maps those to geometry.

## Not established

- The wsd/wpd name binding, as above.
- What `mulli x, y, 0x4050` strides over — a runtime structure of 16,464 bytes,
  twice in this function, unread.
- The Kind/SetNo → model resolution.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**Land cycle 1489's video, then draw the trees as points.** 35,846 positions
with a verified world transform (cell * 8192 − 65536 + 4096 + local, the same
centring as the .pdl) can go into the flight sequence as billboards or dots in
one tool change, and the picture will say immediately whether the transform is
right — the same control that caught the placement list's origin at 1447.
