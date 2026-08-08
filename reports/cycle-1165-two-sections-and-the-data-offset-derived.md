# Cycle 1165 — the data offset is derived, and the "stride" was a section base

## The function that settles both

`0x8234B268(descriptor, section, level)`:

```
8234b274  cmplwi cr6,r4,0x1
8234b278  beq    cr6,0x8234b28c
8234b27c  cmplwi cr6,r4,0x2
8234b280  bne    cr6,0x8234b290
8234b284  lwz    r9,0x20(r10)   ; section 2 -> the word  at descriptor +0x20
8234b288  b      0x8234b290
8234b28c  lhz    r9,0xc(r10)    ; section 1 -> the halfword at descriptor +0x0C
8234b290  add    r3,r9,r10      ; base = descriptor + that
8234b298  beqlr  cr6            ; level 0 -> done
8234b29c  addi   r10,r10,0x30   ; else sum the per-level array at descriptor +0x30
```

Two of this workspace's outstanding "measured, not derived" notes close here.

## The data offset

Cycle 1152 wrote that the pixel offset is *"measured with a control rather than
derived"*: the pixel pointer reaches `0x8233EA78` as a separate argument, so
nothing in the texture path read it out of the file. The control was that using
file `+0x30` the surface rule held 308 of 308, and using `+0x28`, `+0x2C`,
`+0x34`, `+0x38` or `+0x3C` it held 0 of 308.

The control was right and it is no longer the evidence. `0x8234B284` reads the
word at **descriptor +0x20**, which is file `+0x30`, and adds it to the
descriptor. Measured across the 86 packs, `descriptor + [+0x20]` is `0x1000` in
**86 of 86**.

## The "stride" was a section base

Cycle 1164 read `0x821D9478` writing `0x50` into the halfword at file `+0x1C`
and called it the record stride. That was one inference too many.

`0x8234B28C` reads the same halfword as **section 1's base**: `descriptor + 0x50
= 0x60`, which is exactly where cycle 1163 measured the first sub-record's
descriptor. It is an offset to the sub-record area, not a spacing between
records. The two happen to coincide at `0x50` in the wrapper `0x821D9478`
builds, which is precisely why the inference was available and wrong.

```
section 1 base (descriptor + [+0x0C]) == 0x60     82 / 86
section 2 base (descriptor + [+0x20]) == 0x1000   86 / 86
```

The spacing between consecutive sub-records is still measured at `0x50` and is
still not derived. Nothing read so far walks record *n*; `0x8234B268` locates a
section and then walks **mip levels** inside it, not sibling records.

## What the four outliers now look like

The same four packs that declare `0x80` at `+0x1C` are the four whose section 1
base is not `0x60`. Under the corrected reading that is not a malformed stride —
it is a wrapper whose sub-record area simply begins somewhere else, which is a
perfectly ordinary thing for an offset field to say. Their `+0x44`/`+0x54` values
were being read at the wrong place, because those offsets were derived from the
assumption that section 1 starts at `0x60`.

That is a better outcome than cycle 1164's: four files are not anomalous, they
are four files this port was reading with a hard-coded base.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed. The decoder still reads the data offset at file `+0x30`,
which is now derived rather than controlled; its header comment says "measured
with a control" and that wording is now understated rather than wrong, so it is
left for a cycle that touches the file for a reason.
