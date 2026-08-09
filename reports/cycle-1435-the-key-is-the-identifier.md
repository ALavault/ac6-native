# Cycle 1435 — the key is the identifier

## Qualification

- **No Ghidra run and no oracle pass.** The product's ports over the package.
- No product C++ changed; ctest stays **53**. **No contract entry** — this
  establishes a join; nothing consumes it yet.
- New: `tools/ndxr_texture_join.cpp`.

## The join

`NdxrTextureRef::texture_id` is documented as "the key into registry
`0x828C8100`" — a runtime table this product does not have. The **file** side of
that key is the NTXR's own **GIDX identifier**, which `ntxr_texture.h` already
locates: `GIDX` sits exactly `0x10` after `eXt`, and the identifier is at
`GIDX+0x08`, verified 346 of 346 there.

Collecting every id the package's materials reference and every identifier its
wrappers carry:

```
materials 1227, texture refs 2161, distinct ids 437
GIDX identifiers across the extracted tree 459
resolved: 360 of 437  (82.4%)
```

**The key is the identifier.** 360 of 437 is not a coincidence available to a
wrong reading.

## What the other 77 are

Fourteen **contiguous runs**, of 2 to 8 ids each:

```
0x10000C67..0x10000C6E (8)   0x10001800..0x10001807 (8)
0x1000187E..0x10001885 (8)   0x10002200..0x10002205 (6)
0x100023A2..0x100023A7 (6)   0x1000246E..0x10002473 (6)   ... 14 in all
```

Runs of that size are **whole texture sets**, and the extraction is `idx_0009`,
one index of the archive. So the unresolved ids are not a broken join; they name
wrappers that are not in this subset.

Every wanted id has high byte `0x10`, while the tree also carries identifiers
from `0x0F000000` — so the high byte is a namespace and textures are one of at
least two.

## And they decode

```
NTXR wrappers that decode to pixels: 82 of 86 (10,731,568 texels)
  refused payload-size-mismatch  4
```

**Not 4 of 86, which is what the first run said.**

## Array 1 is exact for NDXR and padded for NTXR

The first run handed the decoder `array1`'s length and got 4 of 86, all refused
with one named cause. That is cycle 1418's shape — *my span was wrong, not the
reader* — and the asymmetry behind it is worth stating, because cycle 1419
established the opposite for the other container type:

- for an **NDXR**, array 1 is the container's exact content length, and it
  matches the container's own `+0x04` at **292 of 292**;
- for an **NTXR** there is no such field, the sub-entry runs to the next one,
  and array 1 therefore **includes the padding between them**.

So "array 1 is the length" is true of one container kind and not the other, and
a caller has to compute the payload extent from the descriptor —
`0x10 + data_offset + single_level_surface_bytes` — instead. Handing that in
takes 4 of 86 to **82 of 86**.

I generalised from one container type to another without checking, which is the
same class of error as cycle 1430's "read the rest of the struct": the earlier
finding was correct and its scope was assumed.

## Not established

- What the remaining 4 refusals are. They are the same named cause and were not
  read further.
- The `0x0F…` namespace.
- Which of a material's textures is the diffuse one. `texture_count` is up to
  several per material and nothing here orders them.
- Any sampling. The coordinates are decoded, the pixels are decoded, and nothing
  joins them to a pixel on screen.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Sample one.** Every piece is now in hand — UVs on the vertices, a texture id
on the material, an identifier on the wrapper, and 10.7 million decoded texels —
and the rasteriser already interpolates barycentrics. What it does not do is
carry a UV through them and read a texel.

The honest first picture is one model with one texture, not the mission: which
of a material's several textures is the base colour is unread, and picking the
first would be a guess worth naming before it is made.
