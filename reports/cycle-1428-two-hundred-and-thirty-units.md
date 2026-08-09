# Cycle 1428 — two hundred and thirty units

## Qualification

- **No Ghidra run and no oracle pass.** The product's own ports over the
  extracted package and the mission payload.
- No product C++ changed; ctest stays **53**. **No contract entry** — the
  decoder is still uncontracted for the reason cycle 1427 gave, unchanged.
- New: `tools/ndxr_mission_models.cpp` and the capture
  `reports/mission01-native-captures/ndxr-mission-roster/`.

## Every model the mission actually spawns

```
scenario: 230 units, 38 distinct primary model ids
drew 38 of 38, 154351 vertices total
```

**The ids are not chosen here.** They come from the scenario's own model bytes
at `+0x61`/`+0x62` of each Obj record, through `MissionScenario` and
`ModelDirectory` — the join `retail_model_directory_tests.cpp` has been checking
all along at 311 resolved bindings and 38 distinct primaries. Cycle 1427
rendered an id I picked; this one renders the mission's.

The sheet is legible without a caption: fighters, a tank, ships, radar masts,
anti-air, buildings, two long strips, two domes, flat plates, and a
2.4-kilometre terrain.

## The extents are the evidence, not the shapes

| | extent | used by |
|---|---|---:|
| id 2 | 2409 × 170 × 1083, 373 pieces, 41,868 verts | 1 unit |
| ids 18, 19 | ~1480 × ~100 × 435 / 187 | 1 each |
| ids 14, 16, 24, 26, 43, 72 | ~10–15 × ~4.5 × 14–25 | 34, 28, 24, 16, 16, 8 |
| id 4 | 5.40 × 4.56 × 14.52 | 11 |
| ids 48, 56, 64 | 17–28 × 41–47 × 139–257 | 1, 2, 4 |
| ids 0, 6, 20 | ~51–59 × 12–17 × 51–56 | 1, 18, 10 |

Six aircraft-sized models used 34, 28, 24, 16, 16 and 8 times against one
terrain used once: that distribution is what a mission's unit list looks like,
and it falls out of a join nobody tuned.

**The labels in that table are mine and the numbers are the file's.** Calling
id 4 a tank is a reading of a wireframe; calling it 5.40 × 4.56 × 14.52 is a
measurement. The README keeps them apart for the same reason.

## What the sheet is not

- **It is not the mission.** Nothing here places a unit at its spawn position.
  The scenario carries those — `initial_world_position` is parsed and
  `retail_mission_state.cpp` already uses it — but this is a contact sheet.
- **Scale is not comparable between tiles.** Each model is framed against its
  own bounds, so a 2-metre part and the terrain fill the same box. That is why
  the extents are printed beside them rather than left to the eye.
- Positions and connectivity only: no materials, no textures, no winding.

## Two threads, and where they now stand

Thread A flies an aeroplane under retail's own arithmetic, blocked from
1:1 position only by estimate instructions that are refused rather than
approximated. Thread B reaches every model this mission spawns, by index, and
draws it.

What separates them is a placement and a camera, and both are things the
scenario already carries. That is a smaller gap than either thread has crossed
in the last ten cycles — which is worth writing down now, while it is still a
prediction and can be checked against what actually happens.

## Not established

- Nothing new is claimed about the models beyond their vertices and extents. The
  categories above are eyeballed.
- The **secondary** model bytes at `+0x62`. 38 distinct secondaries exist and
  this cycle drew only primaries.
- Whether all 230 units are drawn in Mission 01 or whether some are spawned by
  waves, disabled, or scenery culled at range.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=1
```

## Next

**Place them.** `initial_world_position` already gives a spawn per unit and the
demo already has a camera that follows a flying aeroplane. Drawing the roster at
its spawn positions, seen from the aircraft, is the first frame in which the two
threads are the same picture.

Before that, one honest check the last two cycles have earned: the decoder is
still uncontracted because `0x821FBB10` and `0x821FBA78` were left half-read.
It has now produced 1227 descriptors and 38 models that look like what they
should. That is corroboration, not derivation, and the entry should still follow
the read rather than the pictures.
