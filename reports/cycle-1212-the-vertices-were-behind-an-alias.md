# Cycle 1212 — the vertices were behind an alias, and I had the function on screen

## The answer

**Section 2 is the vertex block and section 1 is the index block, and
`0x82362190` binds both as D3D buffers.** `0x823556E0` calls it unconditionally
as the first thing it does, before any record is walked.

Verified here instruction by instruction:

```
823621a8  lwz  r11,0x10(r31)   ; ctx+0x10 = obj+0x20, the variant marker
823621b4  li   r28,0x4         ;   Usage bit 4 when it is 1
823621b8  addi r29,r31,0x14    ; ctx+0x14 = obj+0x24, an index-buffer header
823621bc  lwz  r3,0x14(r30)    ; Length = [buf+0x14]      <- SECTION 1
823621c8  li   r5,0x1          ; Format = D3DFMT_INDEX16
823621d4  bl   0x821fbb10      ; XGSetIndexBufferHeader
823621dc  lwz  r4,0x74(r31)    ; ctx+0x74 = obj+0x84      <- section-1 base
823621e0  bl   0x821fc070      ; XGOffsetResourceAddress
823621e4  addi r29,r31,0x34    ; ctx+0x34 = obj+0x44, a vertex-buffer header
823621ec  lwz  r3,0x18(r30)    ; Length = [buf+0x18]      <- SECTION 2
823621fc  bl   0x821fba78      ; XGSetVertexBufferHeader
82362204  lwz  r4,0x78(r31)    ; ctx+0x78 = obj+0x88      <- section-2 base
8236220c  lwz  r11,0x7c(r31)   ; ctx+0x7C = obj+0x8C, section 3 — flushed only
```

The three `XG*` functions are identified from their bodies, not their names:
`0x821FBB10` writes `2` (`D3DRTYPE_INDEXBUFFER`) at `+0x00`, refcount 1, fence
−1, and folds `Format<<29` into Common; `0x821FBA78` writes `1`
(`D3DRTYPE_VERTEXBUFFER`) and builds the fetch constant
`(Length & 0x03FFFFFC) | 0x10000002`; `0x821FC070` switches on the resource type
and adds its second argument into the address word.

## Why five cycles said "nothing addresses the vertices"

**The object is aliased.** Everything below `0x82350F08` is passed
`ctx = this + 0x10`, so the section bases are spelled `0x74/0x78/0x7C(ctx)` —
never `0x84/0x88/0x8C(this)`. Cycles 1198 through 1211 all searched the latter.
The same alias explains the string base already known as `lwz r9,0x80(r28)` for
`obj+0x90`; I had that clue in cycle 1199 and did not generalise it.

And worse than a blind scan: **I disassembled `0x82362190` myself in cycle 1200**
and wrote, of this exact function,

> `0x82362190`, called once at the top of `0x823556E0`, reads `[arg+0x10]`,
> compares it to 1 and sets a local to 4, then walks from `arg+0x14`. It is not a
> file reader and I did not follow it further.

Every word of that is true and the conclusion was wrong. It is not a file reader;
it is the **buffer binder**, and it was seven instructions from the answer. I
filed it as uninteresting because it did not read the file, when the thing I was
missing was precisely the code that *stops* reading the file and hands it to the
GPU.

## The draw, and the three fields cycle 1203 could not settle

Slot `+0x28` of both NDXR vtables is a render traversal ending at `0x82364518`:

```
82364544  lbz  r5,0xf(r11)     ; format low byte
82364548  lbz  r4,0xe(r11)     ; format high byte
82364558  lwz  r8,0x4(r11)     ; desc+0x04
8236455c  addi r7,r10,0x34     ;   the VERTEX buffer header
82364560  bl   0x823453d0      ;   -> SetVertexDeclaration, SetStreamSource
82364588  addi r4,r11,0x14     ;   the INDEX buffer header
8236458c  bl   0x821dd188      ;   SetIndices
823648b4  li   r4,0x6          ; D3DPT_TRIANGLESTRIP
823648bc  lwz  r10,0x0(r11)    ; desc+0x00
823648c0  lhz  r7,0x20(r11)    ; desc+0x20
823648c4  rlwinm r6,r10,0x1f,0x1,0x1f  ; StartIndex = desc+0x00 >> 1
823648c8  bl   0x821df2c0      ; DrawIndexedVertices
```

Inside `0x821DD068`, `desc+0x04` is added to the vertex buffer's base address.
Inside `0x821DF2C0`, `StartIndex × 2` is added to the index base and the count in
`r7` goes into bits 31:16 of the draw-initiator word beside the primitive type.

**So `+0x00` is a byte offset into the index block, `+0x04` a byte offset into the
vertex block, `+0x20` an index count, indices are 16-bit and primitives are
triangle strips** — exactly the mapping `native_geometry_raster.cpp` has carried,
unaudited, since before this session.

## Cycle 1203 resolves in the product's favour, and cycle 1198 is refuted

Cycle 1203 set the two readings side by side and concluded neither stride was
established. The extent test settles it. Reproduced here over 179 distinct files
and 4,338 descriptors:

| hypothesis | `desc+0x04 % stride == 0` | `max(desc+0x04 + stride·count) == [buf+0x18]` |
|---|---|---|
| **product: `0x0613`→32, `0x0611`→28** | **4338 / 4338** | **178 / 179** |
| rival: `0x82012C40[6]` = 20 | 4083 / 4338 | **0 / 179** |
| rival: 16 | 4334 / 4338 | **0 / 179** |

The divisibility column is the one cycle 1203 showed cannot discriminate — 16
scores 4334 of 4338. **The extent column is the discriminator**, it could fail,
and it kills both rivals at zero. The single file short of exact is short by
eight bytes, a pad.

**Cycle 1198's `0x14` was never a vertex stride.** `0x82012C40` is materialised
exactly once in the image, at `0x823554C8`, and its single use writes
`desc+0x28 = table[idx] × u16[desc+0x0C]`. `desc+0x28` is **zero on disk in all
4,338 descriptors** — the field my own ported test already asserts is written at
load. It is runtime scratch, and what `20 × count` means is not established.

The real stride comes from an 8-byte table at `0x828711F0` indexed by
`desc+0x0F`/`desc+0x0E`, word 0 the vertex declaration and word 1 the stride.
That table is BSS-resident and built at runtime, so **the stride values are
corroborated by the extent control, not read from the image.**

## The instrument was blind on 8.5% of the code

`Ac6XenonRefs` scans **786,122** defined instructions. `.text`
(`0x82090000`–`0x823D772C`) holds **859,595**. Every negative this session was
therefore taken over 91.5% of the code, not all of it — and the specific
instruction that makes `0x82351060` a vtable draw slot,
`0x82351070 b 0x82355998`, **is absent from Ghidra's listing.**

Which of this session's zeros survive:

- **Cycle 1192, FHM.** Survives. Its load-bearing claim was a *byte* scan of
  memory blocks, which does not depend on the instruction listing. Its secondary
  claim, "no instruction carries `0x4d20`", is now weaker and should not be
  leaned on.
- **Cycle 1207, MATE never parsed.** Survives on the same grounds — the
  byte-pattern search of initialised blocks, with `GIDX`/`NDXR`/`NTXR` controls.
- **Cycle 1205, zero `0x2005` records.** Unaffected; it is a data census.
- **Any "N call sites" count taken from `Ac6XenonRefs`** is a lower bound, not a
  census. That compounds the cycle-1209 lesson: seven literals were not a census
  there because the eighth site existed; here the eighth might not have been
  listed at all.

## Not established, stated plainly

- The stride values themselves, per the runtime table above.
- Which of the two `0x200` constructors retail selects. `0x8234CB58` branches on
  bit `0x4` of its fourth argument at `8234cbc4`; the flag enters through
  `0x82343010` ← `0x82337C68`, which has eight callers, none walked. **It does not
  change this conclusion**: `0x823556E0` has exactly two callers, one in each
  variant, and `0x82362190` has exactly one caller, so the binding runs on both
  paths.
- What section 3 is. `0x82362190` only cache-flushes it in `0x80`-byte lines.
- The `0xFFFF` restart handling in the product's reader, derived from no
  instruction read here.
- That `0x82910C80`'s heap is GPU-visible memory is inferred from context.

## What this changes for JV

The blocker named in cycles 1200, 1203, 1204, 1207 and 1211 is gone. The vertex
and index blocks are located, the descriptor fields are derived, and the product's
existing reader turns out to have had them right. **The remaining gap is the
stride table, and it is runtime state rather than file content** — which is a
different kind of problem from the one JV has been stuck on.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
179 distinct files, 4,338 descriptors; extent control 178/179 against 0/179 twice
```

No product code changed in this cycle; the header's "what this does not do"
section is corrected in the next commit.
