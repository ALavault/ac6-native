# Cycle 1155 — the level count derived, the MATE confirmed, and an unfair sentence withdrawn

## The correction first

Cycle 1152 wrote of `first-linked-0x10002215.ntxr`, whose payload is 507,904
bytes against a 262,144-byte 512×512 BC2 surface:

> Nothing had noticed because nothing was measuring.

That is not true and it was unfair to a predecessor.
`AC6_MATERIAL_TEXTURE_LINK_REPORT.md` describes that same file as carrying

> a bounded 507,904-byte **aggregate** texture-data tail

— the word is *aggregate*, and the byte count is the same one I recomputed. The
report knew the tail held more than one surface and said so.

What is true, and all I should have claimed, is narrower: `probe_ntxr_bc.py`
decodes that aggregate as a single 512×512 surface anyway, because it never
checks a size against the dimensions it reads. The report's caution did not
reach the script. The decoder's refusal is still correct and still worth having;
the sentence around it was not.

## Byte +0x11 is the level count, and now it is derived

Cycle 1153 flagged this as the weak point: I had been calling `+0x11` the mip
count on the strength of its role, while all the image showed was
`0x8234AED8` testing it against 1 to pick the plain context. Reading the
mip-mapped load path settles it.

`0x8234FB98` (mip-mapped) and `0x8234EC38` (plain) call the same function with
the same argument shape, and differ in exactly one slot:

```
                    plain 0x8234EC38          mip-mapped 0x8234FB98
r3   width          lwz r3,0xc(r31)           lwz r3,0xc(r30)
r4   height         lwz r4,0x10(r31)          lwz r4,0x10(r30)
r5   levels         li  r5,0x1                or  r5,r21,r21    <- 0x8234B128
r6                  li  r6,0x8                li  r6,0x8
r7   format word    or  r7,r30,r30            or  r7,r31,r31
                    bl 0x821FBE30             bl 0x821FBE30
```

`r21` at `0x8234FBCC` is the return of `0x8234B128`, which is `lbz r3,0x11(r3)`.
The plain path hardcodes **1** into the slot the mip path fills with that byte.
That is a `CreateTexture(width, height, levels, …)` shape and it makes the name
a derivation rather than a guess.

The mip path also allocates through `0x8233BE20` with `0x1000` in `r4`
(`0x8234FC00`), which matches the observed data offset of `0x1000` on every
multi-level wrapper in the corpus.

This does **not** yet fix cycle 1153's layout model, which still matches 212 of
360 multi-level wrappers and fails on 64×64 with 7 levels. It removes the reason
to distrust the variable the model is fitted to.

## The MATE structure, confirmed independently

`AC6_MATERIAL_TEXTURE_LINK_REPORT.md` states the first aircraft's MATE has seven
materials and 54 batches, a `0x10`-stride material table in region 0 and a
`0x10`-stride batch table in region 1 whose first word packs the sequential
batch ordinal high and the material index low. Checked against
`exports/first-linked-r_f16c.mate` directly:

- header word 1 is `0x00070036` — **7 materials, 54 batches**;
- the region offsets close on the stride exactly: region 0 at `0x30`,
  `0x30 + 7 × 0x10 = 0xA0` which is region 1's offset, and
  `0xA0 + 54 × 0x10 = 0x400` which is region 2's;
- the ordinal equals the sequential index for **all 54** batches;
- material indices run 0–6 and never exceed the declared 7.

The last two are the controls that matter: a wrong stride or a wrong offset
would break the ordinal sequence or push a material index out of range, and
neither happens.

## RETRACTED — the section below is wrong (see cycle 1156)

The asset inventory in this section was produced with `find . -maxdepth 3`, and
the extracted assets are four to six levels deep. The true counts are
**537 `.ndxr` files (179 distinct by content)** and **3 `.mdlp` (1 distinct,
29 MB, in Mission 01's own bundle)**. Only the `.mate` count of 1 was right.
The binding is *not* input-blocked. Retained below unaltered as the record of
the error.

## Why the binding still cannot be written

The report's chain is MATE batch → material → texture id, matched against an
NDXR polygon's texture id and an NTXR `GIDX`. Two of those three inputs are not
in this workspace:

```
*.mate   1  (exports.pre-s0/first-linked-r_f16c.mate)
*.mdlp   0
*.ndxr   0
```

Only the `.ntxr` population is extracted. So step 2d's second half — closing
MATE batch→material→texture→NTXR in the runtime so `MissionTextureBinding`
resolves from retail bytes — is **input-blocked**, not code-blocked. The
archives are present, so extracting the missing pairs is ordinary work; it is
simply work that has not been done and that a decoder cannot substitute for.

## Decided rather than asked

No parser is written for MATE. One file confirms a structure; it cannot
establish a binding whose other two sides are absent, and a `MissionTextureBinding`
fed from a single aircraft would be the hand-written table the ladder warns
about, wearing a parser as a disguise.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed.
