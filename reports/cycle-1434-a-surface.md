# Cycle 1434 — a surface

## Qualification

- **No Ghidra run and no oracle pass.** Product C++ only.
- `Image` gains a depth buffer and a triangle; ctest stays **53**. **No contract
  entry** — a rasteriser ports nothing.
- New capture `ndxr-model-06-c17`; `ndxr-model-04` regenerated solid.

## Solid

`Image` gains `depth`, `clear_depth()` and `triangle()` — barycentric coverage,
interpolated camera depth, a nearer-wins test. `draw_mesh_solid` walks the strip
with retail's `0xFFFF` restart and its alternating winding, and shades each face
flat from the three vertex normals.

The C-17 and the tank are surfaces now rather than wire.

## Two things the rasteriser deliberately does not do

**No backface culling.** No winding rule has been read out of retail, so
discarding a face by its orientation would be inventing one. Both signs of the
barycentric test are accepted and the shade uses `|dot|`, which is what a
two-sided surface looks like. A model with interior geometry therefore shows
some of it — that is the honest consequence and not a bug to hide.

**No perspective-correct interpolation.** The shade is flat per face and the
depth is interpolated linearly in screen space, which is wrong for large
triangles. It is invisible at these sizes and it is written down rather than
discovered later.

## Where the strip's winding does and does not matter

`draw_mesh_solid` keeps the alternation — triangle *n* is `(a, b, c)` and *n+1*
is `(b, d, c)` — because the format has it. **It changes nothing here**, since
neither side is culled and the shade is orientation-independent.

It is kept anyway, for the same reason `retail_flight_input_apply` keeps
retail's call order: "not observable today" is not "safe to drop".

## The C-17 measured against the real one

Its extent is `53.02 × 50.37 × 17.03`. The real C-17 Globemaster III is 53.0 m
long, 51.75 m span, 16.8 m tall.

Cycle 1430 already noted this; it is worth restating now that the render is a
surface, because the agreement is the strongest single check the geometry chain
has. **Nothing in this campaign chose those numbers**, and a wrong stride, a
wrong section base or a wrong component type could not have produced them.

## Not established

- Any winding rule, above.
- The texture. Coordinates are decoded and **unused**: nothing joins a
  `TextureRef` to an `ntxr_texture`, which is the plan's gap 8 and is now the
  only thing between these pictures and textured surfaces.
- The `COLOR` field of the stride-32 format.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=13
```

## Next

**Gap 8: the texture join.** `src/ntxr_texture.cpp` already decodes 668 of the
package's 692 wrappers, `NdxrContainer::TextureRef` gives a `texture_id`, and
its header names the registry that id keys into — `0x828C8100`. The coordinates
are in hand. What is missing is the lookup between them, and it is a read of one
registry rather than a new subsystem.
