# Textured

`c17-textured.mp4` — 120 frames of `o_c17g` carrying its own texture. Stills of
`o_f16c` (id 16) and `o_f18f` (id 24) alongside.

## The chain, and every hop of it is retail's

```
MDLP[6] -> ContainerIndex -> NDXR -> Record(lod1) -> Descriptor
             position   3 x float32 at +0
             normal     4 x float16 at +12
             texcoord   2 x float32 at +20
           Material -> TextureRef -> texture_id 268445283
           == the GIDX identifier of an NTXR in the same package
           -> decode_ntxr_base_level on its COMPUTED extent -> 512x512 texels
```

`texture_id` naming a GIDX identifier resolves 360 of the package's 437
references (cycle 1435). The extent must be computed rather than taken from
array 1, which is exact for an NDXR and padded for an NTXR.

## What is chosen, and each of these is a guess with a name

- **the material's FIRST texture.** A material carries several — the C-17's
  carries two — and nothing has been read that orders them or names one the base
  colour. Taking `TextureRef(material, 0)` is a choice.
- **`swap_16 = true`.** `ntxr_texture.h` makes it an argument rather than a
  constant precisely because "its evidence is visual", and this is that
  evidence: `false` gives green and magenta, `true` gives military greys. It is
  a picture deciding a bit, which is weaker than everything else here and is
  labelled that way.
- **`repeat` wrapping.** 2.7% of the package's coordinates fall outside [0,1]
  and nothing read says how retail wraps them.
- **affine interpolation.** The UV is interpolated linearly in screen space, not
  perspective-correct, so a steep triangle warps. Invisible at these sizes.
- the camera, the light direction, the framing.

## What is absent

- **The second texture.** Whatever it is — normal map, specular, decal — it is
  decoded-able and unused.
- **The terrain.** Entry 2 decodes 370 pieces and 34,923 vertices with bounds
  spanning 2409 units, and renders as something aircraft-shaped and small. The
  mesh is right and the picture is not; the cause is not established and it is
  not this capture.
- Model 4's texture is one of the 77 whose ids name wrappers outside this
  archive index, so the tank stays untextured.

## Reproducing

```
g++ -std=c++20 -O2 -I reconstruction/ace-combat-6/include \
    tools/ndxr_model_textured.cpp \
    -Lreconstruction/ace-combat-6/build -lac6_product_core -o textured
./textured .../001_MDLP.mdlp DIR 6 120
```
