# Cycle 1504 — the store draws without markers

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Qualified cache index SHA-256:
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  It contains 17 bounded records totalling 631,481,632 decoded bytes and stays
  outside the repository.
- This checkpoint consumes only product readers and retail relations already
  qualified in cycles 1497–1503. No diagnostic TSV, replacement model, oracle
  result or generated recompiler output enters the implementation.

## Marker-free product composition

`RetailMission01CpuCompositor` joins the store-backed Mission 01 map bundle to
the common camera table. Geometry, draw commands and source texture spans are
persistent; GIDX-selected NTXR surfaces are decoded lazily once per explicit
byte-swap choice. One depth target receives, in order:

1. the 65,536 terrain cells using the exact shared four-fan topology and atlas
   UV transforms;
2. the retail eight-world-unit MCA/MCI/MCD water query at every covered terrain
   fragment;
3. the 4,226 accepted placed-city commands, each bound by its exact
   `(selector, record_index)`, retail translation, NDXR strip and NTXR GIDX.

The request selects a qualified aircraft loadout; that directly selects the
retail camera group and FOV record. The CPU lane exposes no marker API and never
opens a filename or manifest.

## Reproducible reference

The qualified 320×180 request uses aircraft 1, view 2, the external pose
`eye=(1000,420,-24000)`, `target=(1000,0,0)`, repeat addressing and explicit
16-bit texture swapping. Two renders through the same persistent compositor
are bit-identical:

```text
terrain considered / visible / rasterised       65,536 / 1,817 / 444
terrain candidate / written triangles           58,144 / 3,930
city considered / visible / rasterised            4,226 / 2,720 / 471
city candidate / written triangles              38,049 / 805
terrain / water / city fragment writes     28,770 / 136 / 852
depth and colour coverage                             28,949
decoded terrain atlas pages                           0–5 (6)
decoded placed-map GIDX textures                          136
marker writes                                               0
colour digest                              c3afe49a56218126
depth digest                               6999a5e0c126f899
```

The cold qualification command opens the store, builds all immutable assets,
decodes textures and renders twice in 9.14 seconds with 770,600 KiB maximum
RSS. This is a reference-path measurement, not the Vulkan 720p30 performance
gate.

## Fail-closed boundary

The frame report mechanically distinguishes retail-closed fields from open
choices. Terrain geometry/UV, water bits, city geometry/binding/translation,
camera group and camera FOV value are closed. Opening camera mode and FOV
variant, camera pose, clipping/culling, map distance policy, byte swap,
mip/sampler/alpha state, water material, sky, vegetation and active units are
open. Therefore `marker_free=true` but `jv_eligible=false`.

Zero or excessive targets, an invalid aircraft/view/sampler, a degenerate
camera and a non-opaque approximation colour all fail before rendering. The
test pins the complete metrics and both frame digests, so an unreviewed visual
change cannot pass as deterministic composition.

## Validation

```text
Release build                                                       pass
qualified CPU frame and second-frame replay                          pass
qualified PAL cache / Mission 01 session                             pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb                      66/66
tools/tests                                                         87/87
sealed-cache audit                                                   17/17
mission01-final-gate-v3 --require JF                                  pass
mission01-playable-gate-v1 --require JF                               pass
contract addresses                                                  321/321
contract derivations                                             52, gaps 0
C++ complexity                                                   218 files
contract artefacts                                      146/146 match HEAD
```

## Residual boundaries

JV is not passed. The capture deliberately shows retail terrain and placed
city against a black unresolved sky; it contains neither vegetation nor active
mission units. The next useful closures are the opening camera mode/pose and
the already-derived retail sky/vegetation consumers, followed by the Vulkan
backend. JP, frontend/PAL localisation and the sustained performance gate
remain open; no human controller session is requested yet.
