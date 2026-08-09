# Retail geometry, on screen

`turntable.mp4`, 120 frames at 30 fps: MDLP entry **4** of Mission 01's model
package, turning about its own centre. Four frames are kept alongside as
`.ppm`/`.png`, and `metrics.json` carries their colour hashes.

**It is a tank.** Hull, turret, gun barrel, a small mast. `5.40 × 4.56 × 14.52`
in the file's own units, which is a tank in metres — nothing in this campaign
chose those numbers or that shape.

## What is retail's, and reached how

Every vertex comes through contracted resolution and nothing is searched for:

```
MDLP[4]                       ModelDirectory, ported from 0x8228E9B8
  -> an FHM span
     -> ContainerIndex        ported from 0x82234C18 / 0x82234DD0
        -> NdxrContainer      opened on array 1's exact length
           -> Record -> Descriptor
              vertices  sections.second + vertex_offset, stride T8[hi]+T18[lo]
              indices   sections.first  + index_offset, u16, restart 0xFFFF
```

15 descriptors, 1171 vertices, 1528 indices for this model. Across the whole
package the same path reaches **1227 of 1227** descriptors and decodes all of
them, with 44,298 strip restarts.

The strip is walked with retail's own `0xFFFF` break, which cycle 1426 found
only after three arbitrations failed for rejecting it as an out-of-range index.

## What is invented, and named

- **the camera and the framing.** The model is centred on its own bounds and
  pulled back by 1.6 × its largest extent, so a small part and a large one both
  fill the frame. Nothing about its real scale in the world is claimed by that.
- **the turn.** A full rotation over 120 frames, driven through A3.1's own
  contracted rotation kernel — but the *choice* to turn it, and the tilt, are
  this capture's.
- the colours, the resolution, the field of view.

## Lit by its own normals, since cycle 1433

The segments are shaded by the vertex normal — four `float16` at `+12`, read
after the element tables at `0x8201140C` and `0x820111D8` gave the offsets.
**178,973 of the package's 179,322 normals are unit length and the other 349 are
exactly zero; none is anything else**, which is the control on the reading.

The light direction and the ramp are chosen. This is not shading a surface:
there is no depth buffer and no fill, so a far edge draws over a near one.

## What is absent, and it is most of each vertex

**Superseded at cycle 1433.** The element tables were read, so positions,
normals and texture coordinates are all decoded: the stride-28 layout is
POSITION at 0, NORMAL at 12, TEXCOORD0 at 20.

What remains undecoded is the four-byte `COLOR` of the stride-32 format — eight
descriptors of 1227 carry it — and the Xenos type words themselves, whose
meaning was measured from the data rather than decoded from the code.

**Superseded again at cycle 1434**: the demo gained a depth buffer and a
triangle fill, so this is a solid surface now. What is still absent is the
texture — the coordinates are decoded and nothing samples with them — and any
winding rule, which is why neither face is culled.

The wireframe claims exactly what cycle 1426 established — the positions and the
connectivity — and nothing further.

## Reproducing

```
cmake --build reconstruction/ace-combat-6/build
g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
    tools/ndxr_model_render.cpp \
    -Lreconstruction/ace-combat-6/build -lac6_product_core -o model
./model .../001_MDLP.mdlp DIR 4 120
ffmpeg -y -framerate 30 -i DIR/model-%05d.ppm -c:v libx264 -pix_fmt yuv420p \
    -crf 20 -vf "scale=960:540:flags=neighbor" turntable.mp4
```

The `.png` files are `pnmtopng` conversions run by hand; `metrics.json`'s
`color_hash` is written from the framebuffer, so
`tools/audit_capture_images_match_metrics.py` checks that conversion too.
