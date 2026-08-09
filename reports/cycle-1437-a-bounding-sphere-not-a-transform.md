# Cycle 1437 — a bounding sphere, not a transform

## Qualification

- **No Ghidra run and no oracle pass.** The package and the product's ports.
- No product C++ changed; ctest stays **53**. **No contract entry.**
- `tools/ndxr_model_textured.cpp` gains a record-name filter.

## The hypothesis I brought, and it was wrong

Cycle 1436 ended pointing at one suspicion: that merging 370 pieces into a
single `uint16` index space breaks the terrain, because `0xFFFF` is the restart
sentinel.

**The arithmetic does not support it.** 34,923 merged vertices, per-piece indices
under ~1,000, so the largest renumbered index is around 35,000 — nowhere near
65,535 and never colliding with the sentinel. I wrote the suspicion into a report
as the thing to test and it was refuted by the numbers already in that report.

## What entry 2 actually is

Not a model. A **bundle**, with 114 distinct record names:

```
piece   0  o_1112_lod1     v=7601  x[-1204.7 1204.7] y[-95.8 74.1] z[-768.6 313.5]
piece   2  nmbs001         v=  24  x[  -15.7   15.7] y[ -0.0  1.8] z[ -10.2  -0.0]
piece   3  nmbs002         v=  18  x[  -15.7   15.7] y[  0.0  1.8] z[   0.0   9.1]
   ... nmbs003 .. nmbs368, all within 16 units of the same local origin
```

368 small props at overlapping local positions, plus 196 `hire##` records that
stack the same way. **Drawing every record of a bundle stacks them**, which is
what every multi-piece picture in the last ten cycles has been doing.

## The 32 bytes nobody had read

`NdxrRecord` reads `+0x20` (name), `+0x2A` (descriptor count) and `+0x2C`
(descriptor offset) of a `0x30`-byte record, leaving `+0x00..+0x1F` unread. The
obvious guess was a per-record transform, which would place the props.

It is not. Read for `nmbs001`:

```
f: 0.000  0.922  -5.367  |  16.132  |  0.000  0.922  -5.367  |  0.000
```

and its decoded geometry spans `x[-15.7, 15.7] y[0, 1.8] z[-10.2, 0]` — centre
`(0, 0.9, -5.1)`, corner distance `≈16.5`.

**It is a bounding sphere: centre, radius, then the centre again and a zero.**
The centre matches the geometry's and the radius matches its extent, on a record
picked before the comparison was made.

So **there is no per-record transform**, and the records genuinely share one
space. The stacking is the file's arrangement, not a missing read — these are a
library of interchangeable pieces, not a placed scene.

## And the terrain remains unidentified

`o_1112_lod1` is the bundle's largest record: 7,601 vertices, **2409 × 170 ×
1082**. Rendered alone at four tilts from 0.05 to 1.45 it is a symmetric,
swept, flat shape.

At 2.4 km across it cannot be an aircraft — the C-17's 53 m established the unit
as metres to half a percent. What it depicts is **not established**: at 480×270
with ~2,300 lit pixels I cannot tell a coastline from a wing, and saying which
would be reading a shape into a thumbnail.

The render resolution is the limiting instrument and raising it is one constant.

## What this cycle killed

Two hypotheses, both mine, both by measurement rather than argument:

- the `uint16` merge, refuted by arithmetic already in the previous report;
- the per-record transform, refuted by the record's own bytes matching a
  bounding sphere on the first record checked.

## Not established

- What `o_1112_lod1` is.
- Whether a bundle's records are meant to be drawn together at all, or selected.
  `nmbs001..368` being 368 variants of one 15×2×10 quad suggests selection.
- The second and later `hire##` groups.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Raise the resolution and look.** The renderer is fixed at 480×270 and the one
open question is what a 2.4-kilometre mesh depicts — which is a question a
1920×1080 frame answers and a thumbnail cannot. It is a constant, not a cycle's
work, and it should come before any further theory about what entry 2 is for.
