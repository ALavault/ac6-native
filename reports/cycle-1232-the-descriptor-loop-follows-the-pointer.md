# Cycle 1232 — the descriptor loop follows the pointer now

## The consolidation cycle 1231 named

Cycle 1231 retired the duplicate header parser and said the descriptor loop was
"the obvious next consolidation and it is not done". This does it, and it turned
out to be more than a substitution.

## The difference that mattered

The inline loop walked descriptors **linearly**:

```cpp
offset = polygon_descriptors + polygon * 0x30;   // 0x30 + object_count*0x30 + ...
```

Retail **dereferences**. `0x823555D0` reads `rec+0x2C`, a file-relative offset,
per record. Contiguity is an assumption the file is free to break.

**So the control had to come before the change**, not after. Measured across the
extracted corpus:

```
files: 537
  contiguous walk == pointer walk : 537
  they DIFFER                     : 0
```

The two agree everywhere here, so following the pointer is behaviour-preserving
on this content and strictly closer to retail on any file where it would not be.
Had the count differed, the right move would have been to switch anyway and say
which files changed — but the measurement is what makes that a decision rather
than a hope.

## What the loop is now

```cpp
for (object : records) {
  record = container->Record(object);
  for (index : record->descriptor_count) {
    descriptor = container->Descriptor(*record, index);
    ... stride = descriptor->vertex_stride;
  }
}
```

Five fields that were hand-read with `be32`/`be16` at `+0x00`, `+0x04`, `+0x0C`,
`+0x0E`, `+0x20` now come from the contract-covered reader, and the stride is
read rather than recomputed — the `VertexStride` call cycle 1217 put here is gone
because `Descriptor` already carries it.

## What this file has left

`native_geometry_raster.cpp` was flagged by cycle 1189 for citing zero retail
addresses. After cycles 1217, 1231 and this one, **its NDXR parsing is entirely
delegated** to `NdxrContainer`. What remains local is the vertex decode itself —
positions at `+0`, `+4`, `+8` and UV at `+0x10` or `+0x14` depending on stride —
and **that is still measured**. No cycle has derived the vertex element layout;
cycle 1217 read the declaration tables' strides but explicitly did not establish
the `D3DDECLTYPE` semantics.

So the honest state: the container is derived, the descriptor is derived, the
stride is derived, **and what the bytes inside a vertex mean is not.**

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
537 of 537 files: contiguous walk and pointer walk identical, measured first
```

## Not established, stated plainly

- The vertex element layout, above. The `uv_offset` of `16` or `20` picked by
  stride is a measurement with no address behind it, and it is now the last such
  thing in this function.
- Whether any non-corpus NDXR breaks contiguity. 537 files is the corpus, not the
  format.
