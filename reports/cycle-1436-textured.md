# Cycle 1436 — textured

## Qualification

- **No Ghidra run and no oracle pass.** Product C++ and the package.
- `Image` gains a textured triangle; ctest stays **53**. **No contract entry** —
  a rasteriser ports nothing.
- New: `tools/ndxr_model_textured.cpp` and the capture `ndxr-textured`.

## The picture

`o_c17g` wearing its own 512×512 texture, and two fighters beside it. Every hop
is retail's: geometry, UVs, the material, the texture id, the GIDX identifier it
names, and the texels.

## Four choices, each named where it is made

- **the material's FIRST texture.** The C-17's material carries two and nothing
  read orders them. `TextureRef(material, 0)` is a guess.
- **`swap_16 = true`.** `ntxr_texture.h` makes this an argument rather than a
  constant because "its evidence is visual" — and this cycle is that evidence:
  `false` renders green and magenta, `true` renders military greys. **A picture
  decided a bit.** That is weaker than anything else in the chain and the README
  says so rather than burying it.
- **`repeat` wrapping**, for the 2.7% of coordinates outside [0,1].
- **affine interpolation**, not perspective-correct.

## Two of my own filters, both wrong, both caught by looking

**The record filter.** This tool required `_lod1` in a record's name. The
terrain's records are `hire01`, `hire03`… with no LOD suffix at all, so the
filter excluded every one of them — entry 2 rendered as an aircraft with 2
pieces instead of 370. The roster tool's rule already handled that case and I
wrote a stricter one here instead of reusing it.

**The tilt.** A 2.4 km plane 170 m thick, seen at 0.42 rad, is a sliver: the
first terrain render lit 1,374 pixels of 129,600 and the mesh was correct all
along.

Neither was findable by reading the code and both showed up as a count.

## And the terrain still does not render

With the filter fixed, entry 2 decodes **370 pieces, 34,923 vertices, bounds
spanning 2409 × 170 × 1082** — the census's numbers — and still draws something
small and aircraft-shaped.

**The mesh is right and the picture is not**, and the cause is not established.
What has been ruled out: the record filter, the camera tilt, and the 60,000-vertex
merge cap (34,923 is under it). What has not been examined: whether merging 370
pieces into one `uint16` index space is safe, given that `0xFFFF` is the restart
sentinel and the merge renumbers into the same space.

That is the first thing to test next cycle and it is a five-line experiment:
draw the pieces separately instead of merged.

## Not established

- The second texture of each material.
- The terrain, above.
- Model 4's texture, whose id is one of the 77 naming wrappers outside this
  archive index.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=6
```

## Next

**Draw the pieces separately.** If merging into one `uint16` index space is what
breaks the terrain, the fix is structural rather than a tweak, and it will
affect every multi-piece model — the roster's 373-piece entry is the extreme
case but the tank has 7 and the fighters 4.

The suspicion is specific and cheap to test, which is the right shape for the
first thing a cycle does.
