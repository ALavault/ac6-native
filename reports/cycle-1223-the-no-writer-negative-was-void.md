# Cycle 1223 — the "no writer found" negative was void, and the new instrument voided it

## The negative

Cycle 1218 reported that the byte gating the Mission 01 FSM transition —
`[0x8293BA10] + 0x15A946`, which is `+0x0A` of an embedded `CTaskLoading` — is set
to 1 at `821B9508` and that **no writer was found**. It named its own limit
honestly: *"a `stb rX,0xA(rY)` with `rY` from a non-constant base is invisible to
my scan and I have no positive control for that shape."*

That caveat was correct, and the negative it guarded was worthless.

## The measurement

`Ac6XenonForceScan` over `0x82090000`–`0x823D772C`, with the known write at
`821B9508` as the positive control in the same run:

```
scanned=852724  already_listed=846087  forced=6637  hits=334
control 821b9508 stb r27,0xa(r29)  -> present
stores at displacement 0xA         -> 65
```

**Sixty-five stores** at that displacement, across the whole of `.text`. The
previous scan reported none because it could not see through a register base;
this one does not try to — it lists every store at the offset and leaves the base
question where it belongs, with the reader.

## What one of them is

Three of the 65 sit in the task-gate region cycle 1218 described as reading bytes
`+9` and `+0x0A`. `0x8218D2A0` is six instructions:

```
8218d2a0  rlwinm r10,r4,0x0,0x1f,0x1f   ; mask & 1
8218d2b0  stb    r11,0x9(r3)            ;   -> obj+0x09 = 1
8218d2b4  rlwinm r10,r4,0x0,0x1e,0x1e   ; mask & 2
8218d2bc  beqlr  cr6
8218d2c0  stb    r11,0xa(r3)            ;   -> obj+0x0A = 1
8218d2c4  blr
```

A **flag setter**, taking a mask and writing `1`. `r11` is loaded `li r11,0x1` at
`8218d2a4` and is never anything else. **It sets; it does not clear.**

So the shape of the answer changes. Cycle 1218 asked "is there a writer" and got
a false no. The real question — *does anything ever clear this byte* — is still
open, and now has 65 candidate sites and a known setter to rule out.

## What this says about the instrument

`Ac6XenonForceScan` was built two cycles ago and has now paid twice: cycle 1222
upgraded four uniqueness claims from "in the analysed portion" to "in `.text`",
and this cycle voided a negative that a report had leaned on.

Both times the old instrument was not *slightly* wrong. It returned **zero for
something that has sixty-five instances**, and four cycles ago it returned zero
for an instruction I had disassembled myself an hour earlier.

## Not established, stated plainly

- **Whether anything clears the byte.** 65 stores exist; I checked one. The rest
  need their base register resolved, which the scan deliberately does not do.
- Whether `0x8218D2A0` is called on the `CTaskLoading` at all. It is a generic
  helper on `this+0x09`/`+0x0A`, and its receiver comes from its caller.
- Everything cycle 1218 left open besides this, unchanged.
- **The other "no writer found" negatives in this session have not been re-run.**
  Cycle 1216's `[0x82871084]` enumeration and cycle 1220's
  `[0x8293BA10] + 0x10` are the two that carry weight, and both were taken with
  the instrument this cycle just showed returning zero for sixty-five things.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
65 stores at displacement 0xA over 852,724 instructions; control present
```

No product code changed.
