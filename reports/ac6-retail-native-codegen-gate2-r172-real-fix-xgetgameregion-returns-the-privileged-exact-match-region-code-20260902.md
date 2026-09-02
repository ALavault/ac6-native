# AC6 retail NTSC-U/J — real fix: `XGetGameRegion` returns the privileged exact-match region code (r172)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(156/156, up from 155/155).

## Why this check was worth running

r171 named `XGetGameRegion` as the strongest remaining candidate: its
return value is stored into a table and immediately read back and compared
against several specific constants that gate real control flow, across all
three of this XEX's real call sites.

## What was found

All three real call sites read the return value, and — unlike the first
one — the other two show the **exact** numeric value matters, not just
membership in an accepted set.

**`0x821babdc`** (`sub_821BAB90`): the value is cached, then compared for
equality against exactly four constants, all branching to the same target:

```
821bac10  lwzx r11,r9,r7        ; re-read the cached XGetGameRegion() value
821bac14  cmplwi cr6,r11,0x1ff
821bac18  beq cr6,0x821bac34
821bac1c  cmplwi cr6,r11,0x101
821bac20  beq cr6,0x821bac34
821bac24  cmplwi cr6,r11,0x102
821bac28  beq cr6,0x821bac34
821bac2c  cmplwi cr6,r11,0x1fc
821bac30  bne cr6,0x821bac84
```

Any of `{0x1ff, 0x101, 0x102, 0x1fc}` gives identical behavior at this one
call site (all four `beq`s target the same address).

**`0x821f4a68`** (a cached language-code helper): extracts bits 8-15
(byte1) of the return. `0x101` and `0x102` share byte1=`0x01` and take the
same branch as each other there — which then further distinguishes
`r3==0x101` **exactly** (cached code `20`) from any *other* byte1=`0x01`
value, `0x102` included (cached code `21`, a secondary/generic grouping):

```
821f4a6c  rlwinm r11,r3,0x18,0x18,0x1f   ; byte1
821f4a70  cmplwi cr6,r11,0x1
821f4a74  bne cr6,0x821f4a90
821f4a78  subi r11,r3,0x101
821f4a7c  cntlzw r11,r11                ; normalize(r3 != 0x101)
...
821f4a88  addi r11,r11,0x14             ; 20 if r3==0x101, else 21
```

**`0x821f4b0c`** (a cached region-category helper): extracts bits 16-23
(byte2). `0x101`, `0x102`, `0x1ff` and `0x1fc` all share byte2=`0x01` and
take the same branch — which then distinguishes `r3==0x101` **exactly**
(category `2`) from any other byte2=`0x01` value (category `7`, again a
secondary/fallback grouping):

```
821f4b10  rlwinm r11,r3,0x0,0x10,0x17    ; byte2
821f4b14  cmplwi cr6,r11,0x100
821f4b18  bne cr6,0x821f4b34
821f4b1c  cmplwi cr6,r3,0x101
821f4b20  bne cr6,0x821f4b2c
821f4b24  li r3,0x2                     ; exact 0x101 -> category 2
821f4b28  b 0x821f4b38
821f4b2c  li r3,0x7                     ; anything else in the group -> 7
```

## Value chosen

`0x101`. This is not an arbitrary pick between two equally-plausible
region codes for a "-us"/"ntsc-uj" target (naming convention alone would
be a weak basis, and this project's own discipline refuses that kind of
guess). It is corroborated by this XEX's **own control flow**: two
independent consumer functions both single out `0x101` as the *privileged,
exact-match* case, with every other accepted code — `0x102` included —
falling to a secondary/fallback grouping (code `21` vs `20`; category `7`
vs `2`). No call site's logic is structured the other way around (i.e.
none treats `0x102` as the privileged case and `0x101` as fallback).

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 156/156
(155/155 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. `XGetAVPack` (`0x821f5d14`) and `XGetLanguage` (`0x821f5d9c`), each one
   real call site, are not yet checked for the same category of gap.
2. `VdGetCurrentDisplayInformation` struct+0x05 (r170) remains named but
   unimplemented.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
