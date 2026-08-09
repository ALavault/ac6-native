# Cycle 1427 — it is a tank

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- ctest 52 → **53**. **No contract entry** — see *Why this is not contracted yet*.
- New: `retail_ndxr_geometry.{h,cpp}` and its data-driven tests,
  `draw_mesh_wireframe`, `tools/ndxr_model_render.cpp`, and the capture
  `reports/mission01-native-captures/ndxr-model-04/`.

## The picture

MDLP entry 4 of Mission 01's model package: **a tank**. Hull, turret, gun
barrel, a small mast. `5.40 × 4.56 × 14.52` in the file's own units, which is a
tank in metres.

Nothing in this campaign chose those numbers or that shape. They are what comes
out when the addressing cycle 1426 arbitrated is followed.

```
MDLP[4]  ->  ModelDirectory (0x8228E9B8)  ->  ContainerIndex (0x82234C18/DD0)
         ->  NdxrContainer on array 1's exact length  ->  Record  ->  Descriptor
```

15 descriptors, 1171 vertices. Across the package the same path reaches
**1227 of 1227** descriptors and decodes every one, with **44,298** strip
restarts.

## The binder corroborates the arbitration

Cycle 1426 chose `sections.second` for vertices and `sections.first` for indices
by cross-match — 1227/1227 against 974 and 907, and 1227 against 292 — with no
retail function read to confirm which section is which.

`0x82362190` was opened this cycle. It binds **exactly two** buffers, from
`[r31+116]` and `[r31+120]`, and `r31` is the container plus `0x10` — so those
are `sections.first` and `sections.second`, the same two the arbitration chose
between.

That is corroboration, not derivation, and it is recorded as the pair it is: the
binder says there are two and which fields hold them; the arbitration says which
is vertices and which is indices. The two creators, `0x821FBB10` and
`0x821FBA78`, are thin wrappers and were not followed further — naming which is
the index buffer would close it, and this cycle did not.

## Two sign errors, both caught by looking

The first turntable drew **zero pixels**. `project` refuses anything with
`z <= 1`, so the forward axis is positive and the model has to be pushed *away*
along it; I had subtracted the distance.

The second drew 3168 pixels at frame 0 and **zero at frames 30 and 60**. I was
handing the basis to `draw_segment`, which rotates the *camera* — so the model
swung out of frame instead of turning. The basis now applies to the point after
centring and before the push, and the mesh spins in place.

Neither was subtle and neither was findable by reasoning about the code: both
showed up as a pixel count, and the second only because I counted three frames
instead of one. A single-frame check would have passed the broken version.

## Why this is not contracted yet

The decoder reproduces an addressing that was **arbitrated from data**, and the
one retail function that would settle it was opened and left half-read. Under
this campaign's standard that is `static` evidence plus a strong cross-match, not
a derivation — the same position cycle 1420 took on the FHM table before
`0x82234C18` was found, and it was right to wait then.

The tests are real and they run over 1227 descriptors; the contract entry should
follow the reading of `0x821FBB10` and `0x821FBA78`, not precede it.

## What is absent, and it is most of each vertex

Strides are 28 and 32; the decoder reads the first twelve bytes as three
big-endian floats and discards the rest. Texture coordinates and normals live
behind `T8`'s and `T18`'s element pointers, unread.

That is why this is a **wireframe and not a surface**. There is no material, no
texture and no winding rule, and filled triangles would imply all three. The
wireframe claims the positions and the connectivity, which is exactly what has
been established.

## Not established

- Which of `0x821FBB10` / `0x821FBA78` creates the index buffer.
- The element layouts inside the two vertex formats.
- Whether entry 4 is a tank *in Mission 01* — it is a tank in the package, and
  nothing here joins it to a spawned unit. `binding.primary` does that and is
  already tested; this cycle rendered an id chosen by hand.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
ndxr-geometry tests                   1227 descriptors, 1227 decoded, 44298 restarts
capture_images_match_metrics          pass compared=4
```

## Next

**Render what Mission 01 actually spawns**, not an id I picked. The scenario's
`model_bindings` are parsed and tested — 311 resolved, 38 distinct primaries —
so the join exists; what is missing is walking those 38 and drawing them. That
turns "a model from the package" into "this mission's models" and is the last
step before the two threads meet.
