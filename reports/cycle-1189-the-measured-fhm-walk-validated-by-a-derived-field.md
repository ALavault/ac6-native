# Cycle 1189 — the measured FHM walk, validated by a field from another format

## The check that was available

`NativeGeometryDatabase::load_verified_binary` parses an NDXR header, and its
first act is a rejection:

```
be32(4, declared_size);  if (declared_size != raw.size()) return false;
be16(0x0a, object_count); if (object_count == 0)          return false;
be32(0x10 / 0x14 / 0x18 / 0x1c, header/polygon/vertex/additional sizes);
```

**An NDXR carries its own byte count at `+0x04`.** That is a field of the NDXR
format, read by the product, used to refuse a malformed slice — and it is
completely independent of how the slice was cut out of its bundle.

So it is a check on the FHM walk. If `tools/ac6_fhm.py`'s measured layout —
count at `+0x10`, offsets at `+0x14`, sizes after them — were wrong by a single
byte anywhere, the extracted mesh's declared size would not match its length.

## The result

```
meshes extracted from Mission 01's MDLP                        292
  declared size at +0x04 == file size, object count > 0        292
  failing the product's own header check                         0
object counts   min 1   max 110   median 1
```

Every one. The FHM child boundaries are byte-exact on all 292 meshes, checked by
a field the FHM format knows nothing about.

## What this does and does not change

**Does not**: make the FHM layout derived. Cycle 1175's judgement stands and the
walker stays out of the product. A measured format validated by an external field
is still a measured format, and importing one into a place the auditor reads as
derivation is the thing being refused.

**Does**: make the measured layout considerably harder to doubt. It was already
94-of-94 bundles parsing with zero notes; it is now also 292-of-292 slices whose
independently-declared sizes agree. Two formats, two conventions, no disagreement.

**And**: removes part of the price cycle 1186 counted. That cycle found the
diagnostic render blocked behind declaring `vertex_count` and `index_count` for
the drawable contract, and called it "an NDXR header parse in the harness". The
product already contains that parse — `0x10`, `0x14`, `0x18`, `0x1c`, object
table at `0x30` — so the harness would repeat a reading the product carries
rather than invent one.

**Correction, made the same day.** The paragraph above first called those
offsets *derived*. They are not. `src/native_geometry_raster.cpp` and
`include/ac6/native_geometry.h` cite **zero retail addresses** between them, and
no contract names either as a derivation. The NDXR header layout in this product
is exactly as measured as the FHM layout is.

So the price cycle 1186 counted is **not** removed — it is relocated. The harness
would not be repeating a derived reading; it would be repeating a second measured
format alongside the first. And the more useful thing this exposes is that the
NDXR decoder, which has been in the product since long before this session, rests
on an unaudited format description. That is a bigger open item than the one this
cycle set out to close, and it is now written down.

I caught it by checking a claim I had made one cycle earlier rather than
inheriting it — which is the same habit that produced four corrections this
session and the only reason this one took minutes instead of twenty cycles.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
three live contracts                                ->  audit-valid
```

No product code changed. Extracted meshes are retail bytes and stay local.
