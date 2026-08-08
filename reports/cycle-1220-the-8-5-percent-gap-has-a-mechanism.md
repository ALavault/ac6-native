# Cycle 1220 — the 8.5% gap has a mechanism, and the creator lead was a displacement collision

## The instrument, finally explained rather than measured

Cycle 1212 recorded that `Ac6XenonRefs` scans 786,122 instructions while `.text`
holds 859,595, and left the gap as a brute fact. Here is the mechanism, caught
live.

Searching for the substring `0x1ad8` returns **zero**. The instruction

```
821b54c8  lwz r11,0x1ad8(r11)
```

exists — I disassembled it in the same session — and its text contains `0x1ad8`.
A bare `1ad8` in the same run returns eleven hits, so the matcher works.

**`Ac6XenonRefs` iterates `getListing().getInstructions(true)`, which yields only
instructions Ghidra has already disassembled.** `Ac6XenonDisasm` calls
`disassemble(addr)`, which creates them on demand. So the 8.5% is not random
coverage loss: it is **every function Ghidra's auto-analysis never reached**, and
the two scripts in this repository disagree about the program by exactly that set.

This sharpens the rule from "state the denominator" to something actionable:
**a negative from `Ac6XenonRefs` is a negative over analysed code only, and
`Ac6XenonDisasm` on a suspected address will tell you in one command whether the
address is in the gap.** That check costs nothing and I had not been making it.

## What `0x821B54C0` does

```
821b54c0  lis r11,-0x7d97
821b54c8  lwz r11,0x1ad8(r11)   ; [0x82691AD8] — creator table entry 0
821b54c4  lis r10,-0x7d6c
821b54cc  lwz r10,-0x45f0(r10)  ; [0x8293BA10] — the CTaskModeManager
821b54d0  stw r11,0x10(r10)     ; -> manager+0x10, the creator pointer
821b54d4  blr
```

Six instructions: **set the mode creator to table entry 0**. Cycle 1218 had this
as the seed and it is confirmed.

## The named lead was a displacement collision

Cycle 1218 reported that "only five instructions touch displacement `0x1AD8`" and
named `0x821C5E00`-ish as the next place to look for what selects entry 45. Read
here:

```
821c5e94  bl   0x82222e98        ; an allocation
821c5e98  or   r4,r3,r3
821c5ea0  stw  r4,0x1ad8(r26)    ; r26 is NOT 0x82690000
```

`r26` is an object base, not the table's page. **This writes some other object's
field at the same displacement**, from a pointer that was allocated two
instructions earlier. It has nothing to do with the creator table.

So the five sites are two unrelated things sharing a number, and the "next place
to look" points at the wrong one. **What selects creator index 45 is still
unknown**, and the lead that was supposed to close it is retired.

That is the same shape as cycle 1198's `0x118` quotient and cycle 1208's
threshold: a number appearing in two places, taken for one thing.

## Not established, stated plainly

- What writes `[0x8293BA10] + 0x10` with anything other than entry 0. `0x821B54C0`
  is the only writer found, and it is the seed.
- Whether the real writer is in the analysed portion of `.text` at all. Given the
  mechanism above, a scan that missed it would look identical to a scan over a
  program that does not contain it.
- The gate byte at `[0x8293BA10] + 0x15A946`, untouched here.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
"0x1ad8" -> 0 hits; "1ad8" -> 11 hits; 821b54c8 contains "0x1ad8" and is absent from the listing
```

No product code changed.
