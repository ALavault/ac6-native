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

The count is the point. **Ten of 230 units land on screen**, because their
positions are still the unframed `Obj` offsets: cycle 1142 found the placement
chain but not the frame it is relative to, so most units sit at or near the
origin while the camera follows the player. The picture shows the defect the
reports could only describe.

| | live (tick 900) | debrief (tick 1800) |
|---|---:|---:|
| world markers on screen | 10 | 11 |
| active units | 230 | 230 |
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

## Provenance

| artifact | SHA-256 |
|---|---|
| `retail-session-hud.json` | `6779d64be39c9a98131d1462f862816fe9557f5cd64b5d7f94081cfb0139cb99` |
| `hud-live.png` | `a32f4e600523a2b09ec0ce47c71ddbf3d76cabad231c3dde4202976e057ac877` |
| `hud-debrief.png` | `51bb2747c5759095c77e6d823cce96439608f538aaa65653904a2eeace427dd6` |
