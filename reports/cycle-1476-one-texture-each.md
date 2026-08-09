# Cycle 1476 — one texture each

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ unchanged; ctest stays **59**.
- **No contract entry.**

## The hypothesis, and its own control killing it

Cycle 1475 ended with grey faces and proposed the cause: the renderer takes one
texture per **part** where the container exposes four material slots per
**descriptor**, so most geometry was being drawn with some other descriptor's
skin.

Regrouped per descriptor, rebuilt, re-rendered: **the image is identical.**

The measurement that predicted that was already on the page and I had not
read it back. Cycle 1474 counted the ids: **170 models request 170 distinct
texture ids** — one each. Grouping per descriptor cannot change a picture where
every descriptor of a model already shares the model's only texture.

## What the material lookup actually looks like

```
descriptors 4318; no texture found 0
texcoords present 4318, missing 0
first hit by slot: s0=4318
texture_count histogram: 1:4318
```

Every descriptor: **exactly one texture, in slot 0, with texcoords.** There is
nothing coarse about "first slot, first reference" — it is the only reference
there is. The four slots and the texture count exist in the format and this
package uses neither.

## So what the grey faces are

Not a material problem. Measured over all 170 models:

```
triangles 57479  degenerate 1  edge>100 6562 (11.42%)  edge>400 256 (0.445%)  worst 713.5
```

**One** degenerate triangle in 57,479, so the strips are not stitched by
degeneracy. And 0.445% of triangles have an edge longer than 400 units against
parts capped near 512 — those are the large dark planes, and they are exactly
what walking consecutive index triples as a triangle strip produces from this
data.

Which leaves one unexamined assumption: **that every descriptor is a strip.**
`decode_ndxr_descriptor` assumes it, and cycle 1426 established `0xFFFF` as a
restart — but that establishes the sentinel, not the primitive. A descriptor
field saying "list" or "fan" has never been looked for, and a list read as a
strip produces exactly this: correct vertices, plausible textures, and spurious
triangles spanning the gaps.

## Not established

- The primitive type. Named as the next thing to read, not assumed.
- Whether the 0.445% are spurious at all. A bridge deck 700 units long is a
  plausible triangle; the count is a signal and not a verdict.

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

**Read the descriptor for a primitive type.** `NdxrDescriptor` is ported from
`0x82364518` and the draw at `0x823648C4` computes a StartIndex from
`index_offset >> 1`; whatever that draw passes as the primitive is in the same
window and has never been read. It is the difference between a city with
spurious spans and one without, and it is one function already in hand.
