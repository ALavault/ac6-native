# Cycle 1489 — the seventh page

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ unchanged; ctest stays **60**. Tool changes only.
- **No contract entry** — rendering conventions remain choices.

## The white patches, run down instead of guessed at

Cycle 1487 left "white patches at the shoreline" open with two candidate
readings. Both were wrong, and the forensics said so before the cause was found:

- the brightest tile any shoreline land-cell references has mean brightness
  **125** — pale farmland, `p1 t169`. Nothing referenced is white, so "beach
  tiles rendered correctly" could not survive;
- and 424 distinct (page, tile) pairs cover the shoreline, so a single misread
  cell could not account for patches that large.

The cause is the seventh page. Decoding all seven:

```
pages 0..5   4096 x 4096, fmt 20, 13 mips
page  6      4096 x 1024
```

**Page 6 is a quarter-height page** — and `.mti` knows it: its page-6 entries use
tiles **0..39** only, rows 0..2, exactly what 1024 pixels holds at 272 a row.
My UV maths divided every page by 4096, so page 6's rows smeared into its bottom
edge, and its bottom edge is bright.

The fix addresses tiles in pixels over each page's **own** dimensions. The
shoreline now shows what page 6 actually holds: **shallow-water and shore
tiles**, pale blue, fringing the bay exactly where the water bit meets the land.
They were shore tiles all along — mine were just unrecognisable.

The `272/4096` constant from `[this+0x6D74]` stands for the square pages;
whether retail special-cases page 6's height or normalises per-texture in
hardware is not established.

## The flight, textured

`tools/mission01_flight_sequence.cpp` is rewritten around the full asset chain —
the contracted flight model driving the camera over the atlas ground, the
class-filtered placed parts with their own textures at the mapset's own draw
distances (16000/12000/10000 by class), the `.sph` sky, and the mapset
post-process per frame.

`reports/mission01-terrain/mission01-flight-textured.mp4` — 3,600 ticks at
60 Hz, 1,800 frames at 30 fps, from the farmland north of the city across
Gracemeria and out over the bay.

Every spatial value in every frame comes through a contracted decoder; the
invented list is unchanged and short: speed, stick programme, camera, light and
fog curves, UV inset, sky-row orientation, and which of `at64`/`at72` is north.

## Not established

- Retail's handling of the quarter-height page, as above.
- The remaining fog defect: distance still darkens rather than hazing, because
  `triangle_textured` multiplies. A fog colour needs to reach the rasteriser.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**Fog as a colour, not a multiplier.** It is the one invented curve that is also
visibly wrong — the horizon goes black where the mapset's own `.sky1.fog` says
it should go to haze — and fixing it means one parameter on
`triangle_textured` and its callers.
