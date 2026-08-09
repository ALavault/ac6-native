# Cycle 1433 — position, normal, texcoord

## Qualification

- **No Ghidra run and no oracle pass.** The image's `.rdata` tables and the
  package.
- Product C++ extended; ctest stays **53**. **No contract entry** — the decoder's
  entry is not reopened; see below.
- `ndxr-model-04` regenerated, lit by the normals it now decodes.

## The element tables, read

`T8` and `T18` are `{u16 stride; u16 count; const elems*}`, and the elements are
the Xenon `D3DVERTEXELEMENT9` — **twelve bytes** each, not eight, because its
`Type` is a DWORD. The two formats Mission 01 uses:

```
T8[6]  @ 0x8201140C   offset  0  usage 0   POSITION
                      offset 12  usage 3   NORMAL
T18[1] @ 0x820111D8   offset  0  usage 5   TEXCOORD0     -> stride 28, x1219
T18[3] @ 0x820111FC   offset  0  usage 10  COLOR
                      offset  4  usage 5   TEXCOORD0     -> stride 32, x8
```

T18's elements are appended after T8's stride, so stride 28 is POSITION at 0,
NORMAL at 12, TEXCOORD0 at 20.

## The component types are measured, not decoded

The Xenos format words — `0x002A23B9`, `0x001A2360`, `0x002C23A5` — are **not**
decoded here. What each field is was settled by reading real vertices:

```
+12 as four float16 : -0.352  0.862  0.364  1.000   -> |n| = 1.000
                      -0.122  0.992  0.001  1.000   -> |n| = 0.999
+20 as two float32  :  0.2375 0.3906
                       0.0250 0.7587
```

**A unit vector with w = 1, and a UV pair in [0, 1].** Neither falls out of a
wrong reading, which is what makes them evidence rather than a plausible guess.

## The control, and it is exact

Over all 179,322 vertices of the package:

```
normals 179322 (zero: 349, non-unit non-zero: 0)
texcoords 179322 (outside [0,1]: 4880)
```

**Every normal is unit length or exactly zero. None is anything else.** Bytes
read as the wrong type give a spread of arbitrary lengths, not a clean partition
into two cases — so 0 of 179,322 in the third case is the reading's proof, and
the test asserts it rather than printing it.

The 349 zeros are degenerate vertices and are counted, not tolerated silently.
The 4,880 texture coordinates outside [0,1] are 2.7% and are tiling; that one is
counted rather than asserted, because a model may legitimately tile.

## Why this does not reopen the contract entry

`retail_ndxr_geometry` was contracted last cycle on positions and connectivity.
This adds two fields to the struct it returns and changes nothing it asserts —
the addressing, the strip restart, the length source and the section assignment
are all as contracted.

The entry's statement does not mention normals or texture coordinates, so it is
not made false by their arriving. Widening it is a separate decision and the
right time is when something *consumes* them: right now one demo shades a
wireframe with them, which is a picture, not a behaviour.

## Not established

- The Xenos type words. Their meaning here is measured from three fields in one
  package; another format would need the same treatment or a real decode.
- `COLOR`, four bytes at +20 of the stride-32 format, in 8 of 1227 descriptors.
- The winding rule. Nothing here reads one, which is why the picture is still a
  wireframe and not a filled surface.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
ndxr-geometry tests                   1227/1227, normals 179322, non-unit non-zero 0
capture_images_match_metrics          pass compared=4
```

## Next

**A depth buffer and a triangle fill.** Every input a solid render needs is now
decoded — positions, normals, strips with their restart — and the only thing
between the current pictures and shaded surfaces is a rasteriser the demo does
not have. It ports nothing and claims nothing, which makes it cheap and makes
its absence the honest reason the pictures are still wireframes.
