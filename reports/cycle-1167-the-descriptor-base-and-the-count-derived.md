# Cycle 1167 — the descriptor base and the texture count, derived

## Two constants stop being conventions

`0x8234B0B8` onward is a cluster of one-line accessors over an NTXR file:

```
8234b0b8  lhz  r3,0x6(r3)   ; the texture count      - file +0x06
8234b0bc  blr
8234b0c0  addi r3,r3,0x10   ; the descriptor base    - file + 0x10
8234b0c4  blr
8234b0c8  lbz  r3,0x5(r3)   ; a byte at +0x05
8234b0cc  blr
8234b0d0  lbz  r3,0x4(r3)   ; a byte at +0x04
8234b0d4  blr
```

**`0x8234B0C0` is the descriptor base.** `include/ac6/ntxr_texture.h` has carried
`kDescriptorBase = 0x10` since cycle 1152, justified by the field offsets of
`0x8234B360` fitting when read against it — which is an argument from
consistency, not a reading. It is now read: the accessor returns `file + 0x10`
and every offset in `0x8234B360` is relative to what that returns.

**`0x8234B0B8` is the texture count**, the halfword at file `+0x06`. Cycle 1162
took it from "header word 1's low half", which is the same sixteen bits described
the other way round; now it is the field the code reads.

The two bytes at `+0x04` and `+0x05` have their own accessors and are not
identified here.

## What is left measured

Exactly two things, and they are the same two as after cycle 1166:

- the **`0x50` spacing** between consecutive sub-records;
- the **terminator** convention — one null slot per pack, always last.

`0x8234B268` locates a section and walks mip levels inside it. `0x8234B0D8`, the
accessor immediately after those above, is a second section-base variant
(`r4 == 1` → `lhz +0x0C`, `r4 == 2` → `lwz +0x00`, then `add`). Neither walks
sibling records. The sibling walker has still not been found, and until it is,
522 slots resolving cleanly with exactly one terminator each remains evidence
about the layout rather than a reading of it.

## Where the texture domain now stands

Derived from the image: the descriptor base, the texture count, both section
bases, the data offset, width, height, format code and its 47-entry table, the
mip level count, the cube flag, the surface arithmetic under
`XGSetTextureHeader`, and the chunk layout as retail's own writer emits it.

Measured with controls: the record spacing, the terminator, and the 8-in-16 byte
swap — whose only control remains visual, which is why the decoder still takes it
as an argument rather than hiding it as a constant.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
