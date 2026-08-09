# Mission 01's roster

`roster.png` — every model Mission 01 spawns, 38 tiles, one per distinct
primary model id. `roster.txt` is the tool's own listing: id, how many units
use it, piece and vertex counts, and extents.

**The ids are not chosen here.** They come from the scenario's own model bytes
at `+0x61`/`+0x62` of each Obj record, parsed by `MissionScenario` and joined to
the package by `ModelDirectory` — the join
`retail_model_directory_tests.cpp` already checks, at 311 resolved bindings and
38 distinct primaries. This capture walks those 38 and draws them.

```
scenario: 230 units, 38 distinct primary model ids
drew 38 of 38, 154351 vertices total
```

## What is in it, by its own numbers

| | extent | used |
|---|---|---:|
| terrain (id 2) | 2409 × 170 × 1083, 373 pieces, 41,868 verts | 1 |
| two long strips (18, 19) | ~1480 × ~100 × 435 / 187 | 1 each |
| fighters (14, 16, 24, 26, 30, 43, 72) | ~10–15 × ~4.5 × 14–25 | 34, 28, 24, 16, 4, 16, 8 |
| a tank (4) | 5.40 × 4.56 × 14.52 | 11 |
| ships (48, 56, 64) | 17–28 × 41–47 × 139–257 | 1, 2, 4 |
| buildings (0, 6, 20) | ~51–59 × 12–17 × 51–56 | 1, 18, 10 |
| domes (10, 12) | 59 × 71 × 56 and 18 × 28 × 18 | 16, 10 |
| flat plates (52, 54, 62) | ~7 × ~1 × ~9, and 15 × 1.2 × 19 | 1, 1, 2 |

Nothing here named any of those. The categories are what the wireframes show
and the extents agree with them; the *labels* in that table are mine, and the
numbers beside them are the file's.

## What is retail's

Every vertex, through contracted resolution and nothing searched for:

```
scenario model byte -> ModelDirectory.entry(id)   0x8228E9B8
   -> ContainerIndex over the FHM                 0x82234C18 / 0x82234DD0
      -> NdxrContainer on array 1's exact length
         -> Record -> Descriptor
            vertices  sections.second + vertex_offset
            indices   sections.first  + index_offset, u16, restart 0xFFFF
```

## What is invented

- **the camera, the framing and the pose.** Each model is centred on its own
  bounds and pulled back by 1.7 × its largest extent, so a 2-metre part and a
  2.4-kilometre terrain both fill their tile. **Scale is not comparable between
  tiles** — that is the whole reason the extents are printed beside them.
- the fixed yaw and tilt, the grid, the colours.

## What is absent

Positions and connectivity only. The strides are 28 and 32 bytes and the
decoder reads twelve; texture coordinates and normals are behind element tables
that are not read. No materials, no textures, no winding — which is why these
are wireframes and not surfaces.

The models are also drawn **where they are not**: this is a contact sheet, not
the mission. Nothing here places a unit at its spawn position.

## Reproducing

```
g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
    tools/ndxr_mission_models.cpp \
    -Lreconstruction/ace-combat-6/build -lac6_product_core -o sheet
./sheet .../001_MDLP.mdlp .../000_00_00_00_10.bin roster.ppm
pnmtopng roster.ppm > roster.png
```
