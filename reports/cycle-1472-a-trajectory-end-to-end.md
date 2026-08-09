# Cycle 1472 — a trajectory, end to end

## Qualification

- **No Ghidra run and no oracle pass.** The product and the map container.
- Product C++ unchanged; ctest stays **59**. **No contract entry.**
- New: `tools/mission01_flight_sequence.cpp`.

## The chain, and every link contracted

```
step_flight_session          the stick bends the basis          A3
basis.rows[2]                a unit forward, exactly 0.00e+00   cycle 1470
integrate_session_position   direction + speed -> position      0x82303110
TerrainField                 the ground under it                0x82102568
MapWaterGrid                 land or water                      0x82101EE8
```

3,600 ticks at 60 Hz, 1,800 frames at 30 fps, from `(1000, 900, −34000)` inland
to `(2215, 900, −9039)` over the city — the hills, the river, the coast and the
bay, in one run with nothing between the links but the values they pass.

`reports/mission01-terrain/mission01-flight-over-gracemeria.mp4`.

## What is mine, and it is the same short list every capture carries

The speed (1500 in retail's units), the stick programme, the field of view, the
colours, the light. And which of `at64`/`at72` is north, which is unestablished
and mirrors the map if wrong.

The starting attitude is **not** invented: the run heads `+z` because that is the
identity basis's forward, so no attitude had to be chosen.

## Two things the run shows that a still could not

The heading is held by a **row-1 rotation** — stick 13, paired with `at1252` by
cycle 1471's control matrix — and the path bends by about six degrees over the
minute. That is the model's own turn rate under a config whose limits are mine;
it is gentle because `at1252` is `1.4`, and nothing here tuned it to look better.

And the terrain streams correctly: 96 samples of reach around a moving eye,
12,288 world units, with the coastline arriving at the horizon and resolving as
it approaches. Nothing pops, which is the cheapest evidence that the sample
lattice and the world transform agree at every position and not just at the one
a still was taken from.

## Not established

- Whether the aircraft is at a plausible altitude for this ground. It flies at
  900 and the hills reach 172 here; cycle 1468's question about a shared origin
  is untouched.
- Anything about the parts. The city's buildings are not drawn in this sequence —
  `draw_terrain_view` draws ground and water only.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Put the buildings in the sequence.** The city renderer already draws parts from
the placement list and the sequence already flies over the city; they have not
been run together, and the join is the frustum culling the still renderer does by
distance. That is the last piece between this and a view of Gracemeria from a
moving aircraft.
