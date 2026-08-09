# Cycle 1449 — Gracemeria

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the ported decoders.
- Product C++ **added**: `retail_map_placement.{h,cpp}` and its test.
  ctest **54 → 55**.
- Contract: `mission01-playable-gate-v1.json` gains `retail_map_placement`,
  **32 → 33 behaviours**.

## The port

`.pdl` is `CMapManager+0x28`, its size `+0x5C`, both assigned by the loader
`0x820FBC28` from the name at `0x8205BDE8`. `0x82102148` consumes it: 256
sixteen-byte header records at `coarse_z * 16 + coarse_x`, then a body of
sixteen-byte instances.

**The count/offset reading is enforced, not assumed.** `open` sums the 256 counts
and refuses the file unless the total equals `(size − 4096) / 16`. For Mission 01
both sides are **4,318**. A `.pdl` of four bytes is refused outright, because
`0x820FBF5C` frees exactly that case — "empty" has a spelling.

The world transform is `coarse * 8192 − 61440`, read at
`0x8210220C`..`0x82102220`. Cycle 1447 inferred `cell * 8192 − 65536 + 4096` from
the ±4096 range of the local positions; retail folds the same value into one
constant.

## The control, inside ctest

Every instance is landed on the **independently ported heightfield** — a
different file, decoded by a different retail function — against a deterministic
null model of the same size:

```
instances 4318  ids 173  x -18426..16357  z -20442..6784  flat 98.5% vs null 49.8%
```

The test asserts the **gap**, not the rate, because a rate alone says nothing: a
wrong header reading scatters, and a scatter scores the null model. It also
asserts that every instance lies inside the coarse cell its own header filed it
under, which a wrong offset would break immediately.

Three negative cases are asserted too: a null list, a four-byte list, and a
header whose counts do not partition its body.

## And it draws

`tools/mission01_city_render.cpp` puts the three ported decoders together —
`MapPlacement` for where, `TerrainField` for the ground, `decode_ndxr_descriptor`
for the geometry, parts resolved by integer id exactly as retail's `parts/%d`
does. 331,765 triangles from 129 parts in one view, **zero instances with no
file**.

- `mission01-city-oblique.png` — the city, the coast and the offshore islands
- `mission01-city-skyline.png` — the waterfront from sea level, which is the
  angle Mission 01 opens on

**A rendering error worth recording.** The first render painted the whole city
blue. The ground shader called anything at or below 0.5 "sea" — and cycle 1445
had already measured that the city's ground *is* flat at zero, with the water bit
the only thing that separates it from the bay. Elevation is not the authority on
water; the bit is. The renderer now asks `.mca`/`.mci`/`.mcd` directly. The
finding was four cycles old and I still reached for the wrong test.

**No rotation is applied.** `tag >> 16` is decoded and left unnamed, so every
part is drawn axis-aligned. Buildings that should face along a street do not, and
that is visible in the pictures.

## Not established

- `tag >> 16`. Still unnamed, now with a control available: rotate by the obvious
  reading and see whether the street grids line up.
- Which of the 178 part names each of the 173 ids selects. The renderer resolves
  `part_id` to `%03u_NDXR.ndxr` and every instance found a file, which is
  consistent with an identity mapping and does not establish one.
- The header record's third word; `CMapManager+0x30`.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 33 behaviours
ctest                                 100% passed, 0 failed out of 55
contract_addresses                    pass cited=312 supported=312
contract_derivations                  pass behaviours=51 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Port the water bit.** It is the last of the three grids still read only by
tools, it is the authority the renderer had to reach for outside the product,
and `0x82101EE8` is 152 instructions that cycles 1442 and 1443 have already read
end to end. It is a derivation that is written and a port that is not.
