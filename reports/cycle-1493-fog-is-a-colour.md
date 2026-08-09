# Cycle 1493 — fog is a colour

## Qualification

- **No Ghidra run and no oracle pass.** The product.
- Product C++ **changed**: `Image::triangle_textured_fogged` added, with its
  test. ctest **60 → 61**.
- **No contract entry** — the fog curve remains mine; only the plumbing changed.

## The defect, named at 1487 and fixed here

`triangle_textured` multiplies by one shade, so a fog factor could only darken:
the horizon went to black where the mapset's own `.sky1.fog` (far 24000,
density 0.014) says it goes to haze. The new path blends the lit texel toward a
caller-supplied fog colour instead:

```
out = texel*lit + (fog_colour - texel*lit) * fog
```

`mission01-fog-colour.png` — the same river valley as 1487's overview, with the
distance melting into the `.sph` horizon colour `(126,146,169)` instead of
going dark. The scene finally reads as morning haze rather than dusk.

## The control

Four assertions where the answer is arithmetic, not appearance:

- **fog 0 is the plain path, byte for byte** — the claim that makes switching
  callers safe, asserted as `plain.rgb == fogged.rgb`;
- fog 1 is the fog colour exactly, and pixels outside the triangle untouched;
- fog 0.5 lies between the lit texel and the fog colour;
- **the alpha test survives**: a texel below the cutoff is skipped, not fogged —
  without this, cycle 1478's hangers would return as fog-coloured slabs.

## Seen on the way, and worth recording

The comparison harness drew the water cells from the atlas rather than flat
blue, and the `.mti`'s shallow-water tiles carry the river convincingly — the
per-cell texture assignment covers water too. The flat-blue override in the
tools may be hiding retail's own water tiles; unexamined.

Also: the first attempt at this change made a mess — a broad string replace hit
both copies of the rasteriser body and left the file half-edited. Reset from
HEAD and redone in one deliberate pass; the state that got committed was built
once, not patched twice.

## Not established

- The fog curve itself (exponential in distance/24000, mine) and whether retail
  fogs per-vertex or per-pixel.
- Whether the tools should keep any flat-water override, per the observation
  above.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 61
tools/tests                             Ran 79 tests, OK
```

## Next

**Switch the tools to the fogged path and re-render the sequence** — the video
just landed still multiplies. And with 1489 committed, the tree layers from
1490–1492 can go into the same sequence at the verified transform.
