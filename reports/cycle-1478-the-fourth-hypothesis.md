# Cycle 1478 — the fourth hypothesis

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ **changed**: an alpha test in `Image::triangle_textured`.
  ctest stays **59**, all passing.
- **No contract entry.** The cutoff is mine; the alpha is retail's.

## Looking at one part instead of 57,479 triangles

Cycle 1477 had no fourth hypothesis and said so. Its own next step was the small
one nobody had taken: isolate the offending model and look.

Model **163** is the bridge — 22 records, one descriptor each, named
`mapparts_m01_x_027_separate_*`. Per record the shape is ordinary: 610 triangles
from 1,344 indices with **244 restarts**, worst edge 220.9; 192 triangles with
worst 484.3 against a part capped near 512. Nothing malformed. The geometry is
short strips of small triangles, which is what a girder structure is.

So the slabs were not bad geometry. Looking at where they sat — a curtain
between the main cable and the deck, exactly where a suspension bridge's
**hangers** go — suggested the one thing three cycles of hypotheses had not
touched.

## The measurement

```
models with a decoded texture 170; with >2% of texels alpha<128: 28
most transparent: model 108 at 93.4%
```

**28 of 170 map models carry an alpha-masked texture, and one is 93.4%
transparent.** `Image::triangle_textured` ignored the alpha channel entirely and
wrote every texel opaque, so a lattice — hangers, a fence, a railing, a tree —
became a solid slab of whatever colour the masked-out texels happened to hold.

That is what cycles 1475, 1476 and 1477 were each looking at, and each of us
proposed something else.

## The fix, and what it looks like

```cpp
if ((texel >> 24) < 128) continue;
```

`mission01-scene-alpha-tested.png`: the bridge is a suspension bridge. Towers,
main cables sweeping between them, and the **vertical hangers** in a lattice with
the bay visible through them; the deck's concrete and girders; the city across
the water. Nothing else changed — same textures, same sun, same fog, same draw
distance.

## Four hypotheses, and the one that held

| cycle | hypothesis | outcome |
|---|---|---|
| 1475 | the decoder's size rule is wrong | refuted: untrimmed 177/177 decode |
| 1476 | one texture per part is too coarse | refuted: identical image |
| 1477 | some descriptors are lists | refuted: the primitive is a constant |
| 1478 | the alpha channel is ignored | **28 of 170, and the picture** |

Each of the first three was refuted by a control it built itself, which is the
only reason there was a fourth cycle rather than a wrong fix. And the fourth was
found by looking at **one** model instead of a statistic over 170.

## Not established

- The cutoff. 128 is mine; whether retail alpha-tests, alpha-blends, or does
  both by material is unread, and the material's parameter chain — which
  `retail_ndxr_container.h` ports and nothing consumes — is where that would be.
- The terrain's texture. Still flat colour.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Read the material's parameter chain.** `0x82355358`/`0x82355394` walk it and
the port already exposes the node count; nothing reads the nodes. Whether a
material is opaque, alpha-tested or blended is in there, and it is the
difference between a cutoff I chose and one retail chose.
