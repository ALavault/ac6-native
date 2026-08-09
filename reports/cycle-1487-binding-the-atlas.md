# Cycle 1487 — binding the atlas

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ unchanged; ctest stays **60**. Tool change only. **No contract
  entry** — the UV inset and orientation are choices, named below.

## Every number re-measured before use

Cycle 1486 verified the atlas constants from the instructions. This cycle
re-measured the sweep's data claims against the archive before building:

```
mta: 256 bytes, values 0..23, distinct 24
mti: 12288 bytes = 24 records of 512
byte0 (page): {0..6}      byte1 (tile): 0..224 = 15*15-1, all 225 present
```

And one page decoded: `016_FHM/000` is a **4096 x 4096, format 20, 13 mips**,
and its tiles are aerial photography — fields, tree lines, buildings.

## The binding

Per terrain cell (512 units = 4 x 4 sample quads):

```
record = mta[(gz>>4)*16 + (gx>>4)]
page, tile = mti[record*512 + ((gz&15)*16 + (gx&15))*2]
u = 0.06640625 * (tile%15 + inset + fx * 0.9393382)
v = 0.06640625 * (tile/15 + inset + fz * 0.9393382)
```

**Retail's**: the indexing chain, the step `272/4096`, and the inner fraction
`0.9393382` from `[this+0x6D80]`. **Mine**: that the inset centres the remaining
`0.0607` (half each side — a gutter reading), and the orientation `x -> u`,
`z -> v`. A wrong orientation shows as rotated tiles against their neighbours,
and the renders do not show it, which is consistency and not proof.

Pages decode lazily — 64 MB of pixels each, and a view rarely touches all seven.

## What it looks like

- `mission01-atlas-overview.png` — the river valley from 700 up: photographed
  farmland, forest, and the city's street grid arriving at the waterfront.
- `mission01-atlas-bridge.png` — the bridge with textured Gracemeria across the
  bay.

## Two defects, named now rather than found later

- **The distance fades to black.** `triangle_textured` multiplies by a shade, so
  my fog darkens instead of blending toward the sky. The parts path has the same
  limitation. Fixing it means a fog colour in the rasteriser, not a bigger
  multiplier.
- **White patches at the shoreline.** Cells my water bit calls land whose tiles
  are bright in the atlas. Whether those are beach tiles rendered correctly or a
  page/tile misread at the water's edge is not established.

## Not established

- The UV inset convention and orientation, as above.
- What `[this+0x6D7C]` and `[this+0x6D80]`'s companion constants do.
- The mip chain: only the base level is sampled, at every distance.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**The flight sequence over textured ground.** The sequence tool still draws flat
colour; the scene renderer now has the atlas. Folding the one into the other is
mechanical, and the result is the demo this thread has been building toward
since the reviewer first said it looked like crap.
