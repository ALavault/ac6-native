# Cycle 1429 — ninety-five of two hundred and thirty

## Qualification

- **No Ghidra run and no oracle pass.** The product's ports over the package and
  the mission payload.
- No product C++ changed beyond one new drawing helper; ctest stays **53**.
  **No contract entry** — the decoder is still uncontracted, unchanged.
- New: `draw_mesh_at`, `tools/ndxr_mission_scene.cpp`, and the capture
  `reports/mission01-native-captures/ndxr-mission-placed/`.

## The picture

A four-engine heavy transport at `(1856, 1500, −16416)`, inside a formation of
24 units the scenario places within 2 km of that point. **The model and the
position are both retail's** — the model byte at `+0x61` through the whole
contracted chain, the coordinate from `initial_world_position`.

## The number that shapes the rest of Thread B

```
placed 95 units, 16 distinct models, span 66456 x 9000 x 19024
densest cluster: 24 units within 2000 of (1856, 1500, -16416)
frame 0: 2 of 95 units within 135 of the eye
```

**95 of 230 units have a load-time position.** The other 135 are not drawn —
the container gives them no coordinate, and `retail_scenario.h` already says
that putting them at the origin would be inventing one.

Ground units sit at `y` 0–85 and air units at 700–2000. Nobody assigned those;
they are what the payload holds.

## There is no single picture of this mission, and that is arithmetic

The placed set spans **66 km** and the models are **5 to 50 metres**. An eye far
enough to see the layout draws every unit sub-pixel, and the first survey view
lit **0, 2 and 0** pixels across three frames. Even inside the densest cluster
only **two** units are within 135 of the camera.

A mission map and a model are different pictures. Cycle 1428 said the gap
between the threads was "a placement and a camera"; the placement was there, and
the camera turns out to be the harder half — not because it is difficult to
write, but because no single one shows the thing.

## Three camera errors in two cycles, all found by counting

- 1427: the model behind the eye — `project` refuses `z <= 1`.
- 1427: the basis handed to `draw_segment` rotated the *camera*, so the model
  left frame.
- **1429: the orbit looked outward.** With the eye at `+z` from the centre, the
  offset to the target is negative `z` and every segment was refused. Composing
  the two rotation helpers and hoping they aimed inward produced an empty frame.

The fix was to stop rotating and **construct** the basis: forward from eye to
target, right from world-up × forward, up from forward × right. That cannot aim
the wrong way, where a guessed pair of rotations can and did.

Three of three found by counting lit pixels. None was findable by reading the
code, and all three would have passed a check that rendered one frame.

## What is still wrong in the picture, and visible

**No orientation.** Every unit is drawn axis-aligned. The scenario carries
headings; this does not read them, so the transport and the tanks all face the
same way. It is wrong, it is obvious on screen, and naming it is cheaper than a
reader noticing it first.

## Not established

- The scenario's per-unit heading, above.
- Whether the scenario's `x`/`y`/`z` map to right/up/forward as fed here. `y`
  being altitude is supported by the 0–85 / 700–2000 split; the other two are a
  choice.
- Whether the 135 unplaced units are placed later by waves, or are scenery the
  container positions another way.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=4
```

## Next

**Read the per-unit heading.** It is the one thing between this and a scene that
is not obviously wrong, the scenario carries it, and `ScenarioUnitRecord` is
already parsed — so it is a read of what is beside the position, not a new
subsystem.

Then the decoder's contract entry, which has now waited two cycles on
`0x821FBB10` and `0x821FBA78` and should not wait a third.
