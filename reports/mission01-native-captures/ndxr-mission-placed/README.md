# Placed

`placed.mp4` — 120 frames orbiting the densest cluster of Mission 01's placed
units at a radius of 45. Four frames kept as `.ppm`/`.png` with their colour
hashes in `metrics.json`; `scene.txt` is the tool's own listing.

A four-engine heavy transport, at `(1856, 1500, −16416)`, inside a formation of
24 units the scenario puts within 2 km of that point. **The model and the
position are both retail's.**

## What is retail's

```
scenario Obj record
  +0x61 model byte  ->  ModelDirectory.entry(id)  ->  ContainerIndex
                    ->  NdxrContainer  ->  Record  ->  Descriptor  ->  vertices
  position          ->  initial_world_position
```

95 of the mission's 230 units have a load-time position. **The other 135 are not
drawn** — the container gives them no coordinate, and `retail_scenario.h`
already says that putting them at the origin would be inventing one.

## The layout, in its own numbers

```
placed 95 units, 16 distinct models, span 66456 x 9000 x 19024
densest cluster: 24 units within 2000 of (1856, 1500, -16416)
frame 0: 2 of 95 units within 135 of the eye
```

Ground units sit at `y` 0–85 and air units at 700–2000, which is what the
altitudes look like when nobody assigned them.

**This is why there is no single picture of the mission.** The placed set spans
66 km and the models are 5 to 50 metres, so an eye far enough to see the layout
draws every unit sub-pixel — the first attempt at a survey view lit 0, 2 and 0
pixels across three frames. Even inside the densest cluster only two units are
within 135 of the camera. A mission map and a model are different pictures, and
this is the second.

## What is invented, and named

- **the camera.** An orbit at radius 45, aimed by constructing
  (right, up, forward) from the eye and the target. Retail's gameplay camera is
  `0x82300C20` and cycle 1396 refused to reproduce it; nothing here claims
  retail frames anything this way.
- **the cull.** Units beyond 12 × the orbit radius are skipped: at that distance
  a 15-metre model is under a pixel and costs the frame's time to draw as noise.
- **which axis is which.** The scenario's `x`, `y`, `z` are fed to the renderer
  as right, up and forward. `y` being altitude is supported by the 0–85 / 700–2000
  split; the other two are a choice.
- the colours, the resolution, the field of view.

## What is absent

- **No orientation.** Every unit is drawn axis-aligned. The scenario carries
  headings and this does not read them, so a transport and a tank both face the
  same way — which is wrong and visible.
- Positions and connectivity only: no materials, textures or winding, for the
  reason `ndxr-model-04/README.md` gives.
- The 135 unplaced units, above.

## Reproducing

```
g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
    tools/ndxr_mission_scene.cpp \
    -Lreconstruction/ace-combat-6/build -lac6_product_core -o scene
./scene .../001_MDLP.mdlp .../000_00_00_00_10.bin DIR 120 45
ffmpeg -y -framerate 30 -i DIR/scene-%05d.ppm -c:v libx264 -pix_fmt yuv420p \
    -crf 20 -vf "scale=960:540:flags=neighbor" placed.mp4
```
