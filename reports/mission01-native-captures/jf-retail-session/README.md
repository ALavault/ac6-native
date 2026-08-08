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

| | live (tick 900) | debrief (tick 1800) |
|---|---:|---:|
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
| `retail-session-hud.json` | `e252506c9ccea6a05050724d31edfde9a574bd5f5f6fb539ac12f4e7a7ed4de1` |
| `hud-live.png` | `abaf437844fa4ffb38778eef48125bfeeaf2f9612c6e37f0de03a66bbca1ab22` |
| `hud-debrief.png` | `8a54594ca14e53f91839c04117609acb33f698b6c602a28cb97deac55234f348` |
