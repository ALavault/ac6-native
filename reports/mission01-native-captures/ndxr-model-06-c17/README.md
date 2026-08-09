# o_c17g

`turntable.mp4` — 120 frames of MDLP entry **6**, solid and depth-tested.
Four frames kept with their colour hashes in `metrics.json`.

**The model names itself.** Its NDXR record is `o_c17g_lod1_O_OBJ_O_HIR`, read
from `rec+0x20` against the string table. Mission 01 spawns eighteen of them.

**Its measured extent is `53.02 × 50.37 × 17.03`.** The real C-17 Globemaster III
is 53.0 m long with a 51.75 m span and 16.8 m tall. Under half a percent, on
numbers nobody in this campaign chose — which is an independent check that the
units are metres and that the decoder's absolute addressing is right, since a
wrong stride or a wrong section base could not produce it.

## What is retail's

Every vertex and every normal, through contracted resolution:

```
MDLP[6]  ->  ModelDirectory (0x8228E9B8)  ->  ContainerIndex (0x82234C18/DD0)
         ->  NdxrContainer  ->  Record -> Descriptor
             position   3 x float32 at +0
             normal     4 x float16 at +12, unit length
             texcoord   2 x float32 at +20
             indices    u16 strips at sections.first, 0xFFFF restarts
```

The element offsets come from the vertex declaration tables at `0x8201140C` and
`0x820111D8`; the component types were measured, and the control is that
178,973 of the package's 179,322 normals are unit length, 349 are exactly zero,
and none is anything else.

## What is invented

- **the camera, the framing, the turn and the light.** The model is centred on
  its own bounds and pulled back by 1.6 × its largest extent; the light
  direction and the ramp from a dot product to a colour are chosen.
- **no backface culling, and that is deliberate.** No winding rule has been read
  out of retail, so discarding a face by its orientation would be inventing one.
  Both sides are drawn and the shade uses the absolute value of the light dot.
  A surface with interior detail therefore shows some of it.

## What is absent

- **No texture.** The texture coordinates are decoded and unused: nothing here
  joins a `TextureRef` to an `ntxr_texture`, which is the plan's gap 8.
- The four-byte `COLOR` of the stride-32 format, in 8 descriptors of 1227.
- Any placement. This is one model on a turntable, not the mission.

## Reproducing

```
g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
    tools/ndxr_model_render.cpp \
    -Lreconstruction/ace-combat-6/build -lac6_product_core -o model
./model .../001_MDLP.mdlp DIR 6 120
ffmpeg -y -framerate 30 -i DIR/model-%05d.ppm -c:v libx264 -pix_fmt yuv420p \
    -crf 18 -vf "scale=960:540:flags=neighbor" turntable.mp4
```
