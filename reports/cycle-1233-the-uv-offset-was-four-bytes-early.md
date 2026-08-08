# Cycle 1233 — the UV offset was four bytes early, in both formats

## The last measured thing in the file, derived

Cycle 1232 said the vertex element layout was the only measurement left in
`native_geometry_raster.cpp`. It is derivable, from the element lists the
renderer's own declaration builder walks.

The Xbox 360 `D3DVERTEXELEMENT9` is **12 bytes** — `WORD Stream; WORD Offset;
DWORD Type; BYTE Method; BYTE Usage; BYTE UsageIndex; BYTE pad` — proven by
`0x821DE898`'s walk (`addi r10,r10,0xc`) and by the 12-byte terminator at
`0x820111C0`.

Read directly:

```
T8[6]  0x82011120 : stride 20, count 2, elems 0x8201140C
  0x8201140C  Stream 0  Offset 0x0000  Type 0x002A23B9  Usage 0x00  POSITION
  0x82011418  Stream 0  Offset 0x000C  Type 0x001A2360  Usage 0x03  NORMAL

T18[1] 0x82011138 : stride 8, count 1, elems 0x820111D8
  0x820111D8  Stream 0  Offset 0x0000  Type 0x002C23A5  Usage 0x05  TEXCOORD

T18[3] 0x82011148 : stride 12, count 2, elems 0x820111FC
  0x820111FC  Stream 0  Offset 0x0000  Type 0x001A2086  Usage 0x0A  COLOR
  0x82011208  Stream 0  Offset 0x0004  Type 0x002C23A5  Usage 0x05  TEXCOORD
```

The builder offsets the second list by `T8[i].stride` (cycle 1217,
`823451a4`), so:

| format | layout | TEXCOORD at |
|---|---|---|
| `0x0611` = T8[6] + T18[1] | POSITION@0, NORMAL@12, TEXCOORD@20 | **20** |
| `0x0613` = T8[6] + T18[3] | POSITION@0, NORMAL@12, COLOR@20, TEXCOORD@24 | **24** |

The reading self-checks: T18[3]'s second element declares `Offset 0x0004`, which
is exactly the size of the COLOR before it, and the two sum to the record's
declared stride of 12.

## What the product was doing

```cpp
const std::size_t uv_offset = vertex_stride == 28u ? 16u : 20u;
```

**Four bytes early in both cases.** At stride 32 it read the last word of COLOR
and the first word of TEXCOORD as `(u, v)`. At stride 28 it read the last word of
NORMAL and the first of TEXCOORD.

Corrected to 20 and 24.

## The part that should worry a reader more than the bug

**`ctest` passes 27 of 27 both before and after.**

Nothing in this product asserted a UV value. A wrong texture coordinate on every
vertex of every mesh survived because the tests check counts, strides, bounds and
pixel writes — and the JF capture bundle asserts its numbers before writing its
images, which is exactly the discipline that has held all session, and none of
those numbers is a UV.

So this cycle changes rendering output that no gate observes. That is worth
stating plainly rather than filing the fix as a clean win: **the derivation found
it, and the test suite could not have.**

## Not established, stated plainly

- **The `Type` codes.** `0x002A23B9` (12 bytes), `0x001A2360` (8),
  `0x002C23A5` (8), `0x001A2086` (4) — the sizes follow from the offsets and the
  record strides, and the *formats* do not. The product reads TEXCOORD as two
  big-endian `float32`, which fits 8 bytes and is not established to be right.
  If `0x002C23A5` is a packed type, the values are still wrong and this fix only
  moves where they are read from.
- `0x0711` and `0x0721` are untested against files (cycle 1217) and untouched
  here; their UV offsets are not derived.
- Whether any capture in the repository should be regenerated. The JF bundle's
  images will differ; its asserted numbers will not. **I have not regenerated
  them**, because their hashes are cited by the v3 contract and a regeneration is
  its own cycle with its own before/after.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
element lists read at 0x8201140C, 0x820111D8, 0x820111FC
```
