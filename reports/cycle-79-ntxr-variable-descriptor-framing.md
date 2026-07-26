# Cycle 79 — AC6 NTXR variable descriptor framing

## Evidence

The supplied PAL archive entry 0 contains two bounded NTXR examples that the
fixed 12-word parser could not represent:

- path 0 starts with a 16-word descriptor and `eXt` at `+0x50`, followed by
  `GIDX` at `+0x60`;
- path 37 has raw byte `+0x07 = 2`, but only one contiguous descriptor/eXt/GIDX
  frame before its payload data.

This agrees with the pre-existing full-corpus structure report: descriptor
populations are 12, 16, 20, 24, and 28 words, with first `eXt` offsets from
`0x40` through `0x80`. The raw byte at `+0x07` is retained as a bounded header
property; it is not treated as proof that that many contiguous frames exist.

## Native change

`parse_ntxr` now scans only 16-byte-aligned descriptor lengths from 0x30 to
0x70, requires each `eXt`/`GIDX` pair to retain its checked sizes, and stores
all big-endian descriptor words. It stops after a complete frame when the next
candidate is payload rather than inventing a descriptor there. Word 8 remains
the established allocation offset when nonzero; for the observed zero-word-8
case the bounded data boundary is the end of the complete framed table.

The optional BC1/BC3 decoder remains intentionally fail-closed for exactly the
validated 12-word profile. Longer descriptors are parsed and inventoried, not
decoded as textures.

## Full-corpus result

The bounded manifest now completes within the 55-second limit:

```text
entries=926 rows=56514 ntxr_parsed=7993 ntxr_header_only=13 ndxr_parsed=2228
ndxr_polygon_descriptors=39384
ndxr_vertex_formats=0x0:130;0x6:34551;0x7:2595;0x11:221;0x13:1887
ndxr_uv_formats=0x10:27;0x11:8656;0x12:152;0x13:28628;0x21:1906;0x23:15
```

Every observed NDXR vertex and UV format already selects an existing bounded
parser branch. Therefore no NDXR format implementation was added on a guessed
layout.

## Validation

- `ac6-ntxr-tests`: **1/1 passed**;
- full AC6 CTest: **41/41 passed**;
- root installation retained `bin/` without a nested `bin/bin`;
- `git diff --check` passed;
- no retail asset bytes were added to versioned source.
