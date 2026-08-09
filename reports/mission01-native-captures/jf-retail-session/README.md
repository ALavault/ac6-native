# JF — the retail-driven session HUD

Produced by `ac6-retail-session-tests`, whose only input is the Mission 01
scenario container (SHA-256
`51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`). No
manifest is read on this path, and there is no argument that would let one be.

Unlike the P6 bundle, this one is **not** a fixture: the units, the factions,
the player and the four sub-missions all come out of the retail payload, and
`retail-session-hud.json` records `fixture: false` for that reason.

The test writes `.ppm`; the committed `.png` files are `pnmtopng` conversions of
them, because `reports/**/*.ppm` is ignored. The numbers below are asserted by
the test **before** either image is written — an image that merely looked right
would not get this far.

Since cycle 1144 the capture also draws **the world**, not only the HUD: one
marker per active unit, coloured by the faction byte the retail faction table
gave it, the player larger and white. Markers are a diagnostic lane — no
material, no texture, no topology — and no capture here may be offered as visual
parity.

The count is the point, and cycle 1145 moved it **down**. It was ten of 230,
which looked like a framing defect; the ten were units drawn at the origin
because the origin was what `position_placeholder` returned. Cycle 1145 showed
the `Obj` triple is not a unit's world position at all — `0x8229AF80` places
nothing without a parent and the load path assigns none, and 169 of the 230
triples are `(0,0,0)` — and replaced it with the first tag-2 order of the unit's
program, resolved by `0x822953F0`.

So **95 of the 230 units now carry a real load-time position** and 135 carry
none; a unit with none is not drawn, because the origin is not a fallback but a
different claim. Four are in the live frame, because the session camera follows
a player that has no load-time position and therefore sits at the origin.

`world-overview.png` is the third image and it is a **plot, not a view**. Its
camera is chosen rather than derived — placed from the bounding box of the
derived positions, looking down at their centroid — which is legitimate only
because the marker lane is already declared diagnostic. It exists so the
placement can be checked rather than merely counted, and what it shows is a
mission map: the factions occupy distinct regions, with one isolated group east
of the main engagement.

Cycle 1146 also found why the earlier captures were nearly empty, and it was
not the camera. `project_point` normalises depth as `view_z / 4096`, so every
point beyond 4,096 world units saturates to exactly 1.0 — the value the target
is cleared to — and the depth test `depth >= stored` rejected all of them.
Mission 01 spans 66,000 units. Callers that plot the world now pass their own
far plane; the live view keeps the 4096 that suits content near the player.

| | live (tick 900) | debrief (tick 1800) |
|---|---:|---:|
| world markers on screen | 4 | 4 |
| active units | 230 | 230 |
| units with a load-time position | 95 | 95 |
| distinct spawn coordinates | 59 | 59 |
| markers on the overview plot | 57 | 57 |
| player entity | 4097 | 4097 |
| objectives | 4 | 4 |
| completed | 2 | 4 |
| failed | 0 | 0 |
| outcome panel | hidden | shown |
| HUD pixel writes | 1276 | 1521 |

The two images differ in colour hash, so neither is a copy of the other. The
debrief state was reached because the sub-mission script ran out at tick 1800 —
nothing else in this product completes an objective, and a session whose script
never advances is still in gameplay at tick 1800 with none completed.

This bundle asserts no frame parity with the retail disc. Visual parity is out
of JF's scope by decision of the milestone; comparing an image needs an oracle,
and none was used here.

## The marker count moved from 4 to 29 (cycle 1273)

These images and metrics were regenerated when the rasteriser's default far
plane changed from an invented 4096 to retail's own 24000, read from
`[0x82069B6C]` and stamped at `8225e000 stfs f0,0xe0(r31)` by the camera
manager's initialiser.

The test renders with the **default** plane, so it had been drawing **4**
markers of the 95 units the container places — cycle 1146's original symptom,
still live in this capture. It now draws **29**, and `world_marker_writes` goes
from 36 to 247.

The `.png` files had been stale against their `.ppm` sources by a day: the test
writes PPM, the PNGs are `pnmtopng` conversions run by hand, and nothing checks
that they were re-run. They are regenerated here. **A capture whose image and
metrics disagree is worse than no capture**, because the image is what gets
looked at and the metrics are what get audited.

## Provenance

| artifact | SHA-256 |
|---|---|
| `retail-session-hud.json` | `83caa48e0dcfb9b1aee61a382e167f2c7799fc00234e06dfb279a0befc45adab` |
| `hud-live.png` | `353424ead56c6b986d3000acac2471251a277fb29535733e7b6b8d8a203b2373` |
| `hud-debrief.png` | `8a31275644c0c4d3c7503fbd317b49ef67eea025782f84eb137050d52b3f2c2d` |
| `world-overview.png` | `755b558f67fb42ff1777e542da90d707b1f156a52959d1c022fc6836adff2ee4` |
