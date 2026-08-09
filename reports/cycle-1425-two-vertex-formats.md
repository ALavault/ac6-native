# Cycle 1425 — two vertex formats

## Qualification

- **No Ghidra run and no oracle pass.** The product's own headers and ports, run
  over the extracted package.
- No product C++ changed; ctest stays **52**. **No contract entry.**
- New: `tools/ndxr_geometry_census.cpp`.

## The question cycle 1424 deferred

> Whether `DecodedGeometry` can be produced from an `NdxrContainer` at all. Its
> 26 geometry terms name vertex strides, index sizes and polygon descriptor
> counts; whether the NDXR records carry those is unread. […] If the container
> cannot supply vertex strides and index sizes, the entry point's input is not
> `DecodedGeometry` and the decision needs its shape adjusted **before** it is
> written.

It can supply them, and the shape holds.

`NdxrDescriptor` — already ported, from `0x82364518` — carries every one:

```
index_offset    +0x00   StartIndex is this >> 1
vertex_offset   +0x04
vertex_count    +0x0C
format_hi/lo    +0x0E/+0x0F
index_count     +0x20   16-bit indices, triangle strips
vertex_stride           T8[hi] + T18[lo]
```

So the **index size is 2** and the **topology is strips**, both stated in the
container rather than inferred, and the stride comes from `VertexStride`, ported
at cycle 1217 from `0x82345100` and its two `.rdata` tables.

## And the data is far narrower than the format space

Running the whole chain over Mission 01's package — `ModelDirectory` →
`ContainerIndex` → `NdxrContainer` → every record's every descriptor:

```
containers opened 292, records 1041, descriptors 1227
  total vertices 179322, total indices 256732
  descriptors with stride 0 (format outside the tables): 0
  descriptors with >=3 indices (a strip can be drawn): 1227
  distinct strides: 2  -> 28(x1219) 32(x8)
  format codes: [hi=06 lo=11]x1219 [hi=06 lo=13]x8
```

**Two vertex formats in the entire package.** The tables span 8 × 18 = 144
combinations; the mission uses two of them. And **not one descriptor** has a
stride of zero, which is `VertexStride`'s refusal for a code outside either
table — so the port answers every format the data actually contains.

Both codes check by hand against the tables the header records:

| | T8[hi & 0xF] | T18[((lo>>4)−1)·6 + (lo&0xF)] | sum |
|---|---:|---:|---:|
| `hi=06 lo=11` | index 6 → **20** | index 1 → **8** | **28** ✓ |
| `hi=06 lo=13` | index 6 → **20** | index 3 → **12** | **32** ✓ |

The census reproduces both, which is a control on the ported tables and not just
a count.

## What this settles, and what it costs

`DecodedGeometry` is `{buffer_id, vertices, indices, bounds}` and
`NativeGeometryMetadata` carries the strides, sizes, counts and topology
separately. Both can express what the container holds. So cycle 1424's decision
stands unchanged: the retail-path entry point takes a `DecodedGeometry` and a
metadata record, and drops the 29 bookkeeping terms.

The cost is `DecodedVertex`, which is five floats — `{x, y, z, u, v}` — against
strides of 28 and 32 bytes. Decoding into it **keeps position and one texture
coordinate and discards the rest**: whatever the other 8 or 12 bytes carry,
normals or colours or a second UV set, does not survive.

That is a real loss and it should be declared at the boundary rather than
discovered later. It is also, for a first picture, the right loss: a rasteriser
needs position and a UV, and the alternative is designing a vertex layout before
anything has been drawn.

**The decode is two cases, not a hundred and forty-four.** That is the number
that makes the next cycle small, and it is a fact about this mission's data, not
about the format — a different mission could use other codes, and the port must
keep `VertexStride`'s refusal rather than assume these two.

## Not established

- **What the 8 and 12 trailing bytes of each format are.** The strides are
  derived; the element layouts inside them are `T8`/`T18`'s `const elems*`
  pointers, which the port does not read.
- Whether `index_offset >> 1` and `vertex_offset` are relative to the section
  bases in `NdxrSections`, or to the file. The header says StartIndex is the
  shift; what it indexes into is not stated there.
- Whether 1227 descriptors is all of them — `Descriptor()` returns nullopt on a
  bad record and the census counts only successes, so a silent refusal would
  read as a smaller total. The record and descriptor counts should be reconciled
  against the containers' own declared counts before the number is trusted.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 52
tools/tests                           Ran 79 tests, OK
census                                292 containers, 1227 descriptors, 0 unknown strides
```

## Next

**Reconcile the descriptor count before decoding anything**, because the third
open item above is the kind that turns into a wrong denominator later: compare
1227 against the sum of every record's `descriptor_count`, and 1041 against every
container's `record_count()`. If they match, the census is exhaustive and the
decode can start; if they do not, the gap is a silent refusal and needs its
reason before it is built on.

Then the two decoders, and the first triangle strip on screen.
