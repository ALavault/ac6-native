# Cycle 1446 — ground under the aircraft

## Qualification

- **No Ghidra run and no oracle pass.** The image, the archive and the product.
- Product C++ **added**: `retail_terrain_field.{h,cpp}` and its test.
  ctest **53 → 54**.
- Contract: `mission01-playable-gate-v1.json` gains `retail_terrain_field`,
  **31 → 32 behaviours**.

## What was ported

Cycle 1445 read the heightfield out of `0x82102568`. This cycle turns that into
the product's own decoder, with the derivation in the header the auditor reads:

| | |
|---|---|
| `[this+0x0C]` | 16 x 16 byte grid, indexed `coarse_z * 16 + coarse_x` |
| `[this+0x10]` | patches of `0x4204` = 16,900 bytes |
| within a patch | row step `0x104` = 65 floats, so 65 x 65 |
| one cell | 512 units (`0x82069BB4` = `0.001953125`) |
| one sample | **128 units**, four to a cell |
| the lattice | 1025 x 1025, spanning exactly `±65536` (`0x82069BB8`) |
| absent | `0x82069BC0` = `9990.0`, and NaN bails the same way |

`sample_is_present` is `< 9990.0` and that single `<` reproduces both retail
branches, because the compare at `0x8210272C` continues only on LT and a NaN
sets neither bit. Mission 01's `007_ff_ff_ff_ff.bin` is 16,900 bytes of `0xFF`,
so this is not a hypothetical.

## The control is in the test, not in a citation

The whole 65 x 65 reading rests on the 65th row and column being the shared
edge. `check_shared_edges` re-runs that inside `ctest`:

```
patches 74  edges 31200/31200  height 0.00..487.44  sweep 900/900
```

31,200 samples, worst difference `0.0000`. And a **control on the control**, in
the same test: compare each patch's column 64 against a patch that is *not* its
neighbour and the majority must disagree — otherwise the first number measures
nothing. It does disagree, so agreement is not the default.

Three further assertions that could have failed and did not:

- every one of 1,050,625 lattice samples is present;
- the lattice span equals `2 * kTerrainWorldBias` to within 1e-3 — **the
  heightfield's own extent and the query's `+65536` bias agree without being
  fitted to each other**;
- over a 900-position sweep, a segment one unit below the local ground never
  reports a miss, which is the only direction the early-out's maximum can be
  wrong in.

## What was deliberately not ported

`segment_may_reach_terrain` is the **early-out only**: the maximum over the six
by five neighbourhood against both endpoints' `y`, `0x821027D0`..`0x821027DC`. A
false means the segment provably misses; a true means retail goes on to look
properly, and the looking is VMX128 that cycle 1445 did not read. The header
says so rather than implying an intersection test exists.

## And it draws

`tools/terrain_render.cpp` projects the ported field. The camera, the colour
ramp and the light are mine and are named as mine, the way `DemoCamera` spells
its fields `invented_`; everything spatial comes from `TerrainField`.

- `mission01-terrain-inland.png` — 110,063 quads, hills and a snow line
- `mission01-terrain-coast.png` — 413,863 quads, the coast, the archipelago and
  the bay from the south-west

There has been no ground under the aircraft since cycle 1417. There is now.

## Not established

- Where the map parts sit on it. 178 local-coordinate models and a heightfield
  that does not mention them; the layout link is still the open question it was
  at cycle 1440.
- The rest of `0x82102568`.
- `008` (65 x 65 x 10, all zero), `010`, `011`, `012`, `013`.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 32 behaviours
ctest                                 100% passed, 0 failed out of 54
contract_addresses                    pass cited=307 supported=307
contract_derivations                  pass behaviours=50 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

The artefact checker reports the two new files as `not_committed` until this
cycle's commit lands, which is what it is for; it is re-run after the commit.

## Next

**Put the map parts on the ground.** The heightfield gives every part a `y` the
moment its `x`/`z` is known, so the missing piece is now exactly one thing: the
placement table. `012_00_01_83_b0.bin` and `013_00_02__00.bin` open with what
read as offsets, and `011` shares its first eight bytes with `000` — three
unopened blobs in the same container as the parts, which is the cheapest place
to look and has not been looked at.
