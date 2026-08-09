# Cycle 1467 — ground in the view

## Qualification

- **No Ghidra run and no oracle pass.** The product and the map container.
- Product C++ **changed**: `draw_terrain_view` added to `demo_flight_view`,
  with its test. ctest **56 → 57**.
- **No contract entry**, deliberately: the camera, the colours, the light and
  the haze are invented, and a contract entry would claim otherwise. Everything
  spatial in the picture comes through `retail_terrain_field` and
  `retail_map_water`, which are both contracted already.

## The choice cycle 1466 named

Port the segment query, or go back to the aircraft. I took neither and took the
join: **the flight view has been drawing an invented grid since cycle 1417, and
there is real ground now.**

`draw_terrain_view` takes the same attitude and position as `draw_flight_view` —
the contracted rotation kernel and the contracted integrator — and draws the
map's own heights under them, shaded by the water bit.

## What the test asserts, and the control in it

```
sky 128640  sea 53079  land 48681  water-shading differs in 145242 bytes
```

- sky, sea and land all present;
- **the water grid changes the picture** — rendering with `water = nullptr`
  falls back to an elevation proxy, and 145,242 bytes differ. If the two agreed
  the parameter would be decoration, and cycle 1445 measured the city's ground
  at exactly zero, so they cannot agree;
- off the lattice, nothing is drawn but sky.

## And the first run, which found no land at all

`sky 130560  sea 99840  land 0`, every pixel accounted for. Not a bug in the
renderer: `identity_basis()`'s row 2 is `(0,0,1)`, so the view faces `+z`, and I
had put the aircraft at `z = +2500` — **facing the open sea**. The map is
131,072 units across and the city occupies `z = −20442..6784`; from where I put
it, there was correctly nothing to see.

The test now sits at `z = −9000` over the city and says why in a comment, because
the next reader will otherwise move it back.

## What is still invented, listed in the header

The field of view, the colours, the light direction, the haze — and **which of
`at64` and `at72` is north**, which is unestablished. The view maps `at64` to
world x and `at72` to world z because it must pick something. A wrong choice
mirrors the map; it does not invent terrain.

`reports/mission01-terrain/mission01-flight-view-terrain.png` is the aircraft's
own view of the bay at 800 units.

## Not established

- The aircraft's altitude relative to this ground. `at68` is retail's vertical
  and the terrain is retail's height, but nothing has checked that they share an
  origin — the flight integrator was derived against a ground plane at `y = 0`
  and this map's sea is also `0.0`, which is agreement by coincidence until
  something tests it.
- Anything about the parts. Only terrain and water are drawn.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 57
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Fly it.** The view now draws real ground and the flight model already
integrates a real position; nothing has yet run them together for more than one
frame. A sequence over the bay would test the one thing this cycle listed as not
established — whether `at68` and the terrain share an origin — because an
aircraft that sinks into the ground or floats above it says so immediately.
