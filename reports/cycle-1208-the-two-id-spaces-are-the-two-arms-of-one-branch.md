# Cycle 1208 — the two id spaces are the two arms of one branch

## The discrepancy

Cycle 1207 recorded an open contradiction and refused to explain it away. Cycle
1200 censused texture ids over the **537 standalone `.ndxr` files** and found 170
distinct ids, almost all small (`0x49E`–`0x7EB`). The material agent censused the
**292 NDXR inside `001_MDLP.mdlp`** and found **2,161 of 2,161** in
`0x1000xxxx`. Same field, same walk, two id spaces.

## The split is by content, and the classifier is independent of the field

Classifying each file by its **first mesh name**, read through the string table
this session derived — a completely different field from the ids being counted:

| first-mesh-name prefix | files | id high halves | id range |
|---|---|---|---|
| `mapparts` | **510** | `0x0000` × 12,954 | `0x49E`–`0x2BE1` |
| `tree` | 24 | `0x1000` × 24 | `0x10001C56`–`0x10002C02` |
| `ws11` | 3 | `0x1000` × 36 | `0x10002815` |

**Perfectly clean, with no file mixing the two.** Terrain uses small ids; models
use `0x1000`-form ids. The MDLP's 292 are models, which is why that census saw
one space and mine saw the other.

## And retail has exactly that branch

This is not a story fitted to the numbers. `0x82340870`, the NTXR-pack
registrar, tests the identifier against `0x10000000` and rebases below it:

```
82340908  bl     0x8234b150     ; the entry's GIDX identifier
8234090c  or.    r5,r3,r3
82340910  blt    0x82340968     ; no identifier -> fail
82340914  lis    r11,0x1000     ; 0x10000000
82340918  cmplw  cr6,r5,r11
8234091c  bge    cr6,0x82340928 ;   at or above: use the id as-is, globally
82340920  lwz    r11,0x8(r27)   ;   below: id += [manager+0x08]
82340924  add    r5,r11,r5
8234093c  bl     0x8234bec8     ; register under the resulting key
```

Verified here instruction by instruction rather than taken from the report that
first mentioned it.

**The two id spaces are the two arms of that compare.** Ids at or above
`0x10000000` are global keys; ids below are **pack-local and rebased by the
manager's base**. The boundary is the constant in the image, and every measured
id falls on the side its content kind predicts: `0x2BE1` is the largest terrain
id and `0x10000000` is the threshold, four orders of magnitude clear.

## A correction to the material report's own caveat

That report flagged the rebasing branch as weak evidence, on the grounds that
"all 663 distinct GIDX identifiers in the corpus have high half `0x1000`", so
pack-local rebasing "never fires for these assets".

**That is true of the MDLP corpus and false in general.** It fires for 12,954 of
the 13,014 texture references in the standalone corpus — every terrain mesh. The
branch is not dead; it is the terrain path, and the conclusion was drawn from the
one corpus where it happens not to be taken. Reading two corpora as one is the
same error in a different costume as cycle 1203's, where I aggregated two format
populations and misread a per-format rule.

## Why this matters beyond bookkeeping

A port that resolves texture ids must know which arm applies, and the answer is
not in the NDXR file — it is `[manager+0x08]`, a runtime value belonging to the
pack the texture came from. **Terrain textures cannot be resolved from the mesh
file alone**, even with the whole material chain derived. Models can, because
their ids are already global.

That is a concrete, newly-known constraint on JV, and it was invisible while the
two corpora were being read as one.

## Not established, stated plainly

- What `[manager+0x08]` is set to, and by what. The rebasing base is named and
  unread.
- Whether every `mapparts` id resolves once rebased. I have the ids and the rule,
  not the base, so no resolution was attempted.
- Whether the `tree`/`ws11` split is meaningful or just the two non-terrain
  families that happen to be in this extraction. 27 files is a small sample.
- The `102` MATE materials with `195` ids absent from every NDXR material, from
  cycle 1207. Untouched here.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
537 files classified by first mesh name; 13,014 texture ids; no file mixes spaces
```

No product code changed.
