# Mission 01 native captures

These are compact PNG views of the same native headless run used by the J0
gate. `color.png` is a lossless PNG conversion of the native 1280x720 PPM
readback. `depth-preview.png` is a display-only inverted grayscale view of the
native float depth buffer; the raw F32 buffer remains external.

Run:

```text
SDL_AUDIODRIVER=dummy xvfb-run -a \
  reconstruction/ace-combat-6/build/ac6-native \
  --play-headless /tmp/ac6-mission01-qualified-6lq59r 1 \
  /tmp/ac6-native-evidence/mission01.replay \
  /tmp/ac6-native-evidence/headless-v2
```

Provenance:

- native session: `ac6.native-session.v1`, 1,800 fixed ticks;
- qualified external manifest: `/tmp/ac6-mission01-qualified-6lq59r/manifest.tsv`;
- color PNG SHA-256: `6930379c1e6bfd336bb3e7b88dc68d00e824c06833436eb8cffe4376e1002636`;
- depth preview PNG SHA-256: `d4a23dcae8f752f1f9dc37bc27f8b26b4a00a804e7b3a420b389aab7f003613e`;
- source color PPM SHA-256: `9fd8d3040f6535c8bd35a39a116e9fd3f109004ff39f2fe6427d51ff99edb196`;
- source depth F32 SHA-256: `faa9ad8b7306ed64d26dd292855bc38b2ad50b480b003db78cf9f6632a1dd7db`.

The image is intentionally an honest capture of the current minimal native
renderer: geometry is visible, but the scene is sparse and dark. It is not a
claim of fine visual parity.


## STALE since cycle 1233 — every colour image from the textured path

Cycle 1233 derived the vertex element layout from the renderer's own declaration
tables and found the product had been reading the texture coordinate **four bytes
early on every vertex of every mesh** (TEXCOORD is at `+20` for stride 28 and
`+24` for stride 32; the reader used `+16` and `+20`). A corpus control scores the
derived offset at **99.8% plausible against 0.0%** for both offsets used here.

Cycle 1236 recorded this for `p7-current-main` alone. Cycle 1237 audited the rest:

| bundle | geometry | colour images affected |
|---|---|---|
| `p0-raster-fill` … `p5-raster-recapture`, `p7-current-main` | 3,628–3,727 triangles, 10 drawables | **yes** |
| `p6-native-hud`, `jf-retail-session` | none — HUD and markers only | no |

**Seven bundles, not one.** What does *not* change is every number: triangle
counts, fragment tests, depth-pass fragments, unique pixels, screen bounding
boxes, object IDs and depth ranges are all independent of where a UV is sampled.
Those are what the contracts cite, and they remain valid.

None of these images has been regenerated. They are captures of the **manifest**
path, whose transforms, materials and textures `MISSION01_LADDER.md` records as
synthesised — so a corrected sampling would be a better rendering of something
that is still not Mission 01. The regeneration belongs with JV's fused
retail-session render, and is deliberately not bolted onto a bug fix.
