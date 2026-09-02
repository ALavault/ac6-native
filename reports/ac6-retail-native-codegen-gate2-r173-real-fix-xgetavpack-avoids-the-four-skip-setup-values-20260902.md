# AC6 retail NTSC-U/J — real fix: `XGetAVPack` avoids the four skip-setup values (r173)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(157/157, up from 156/156).

## Why this check was worth running

r172 closed `XGetGameRegion`, leaving `XGetAVPack` and `XGetLanguage` (each
one real call site) as the last unchecked imports in the platform-config
family opened by r171.

## What was found

`XGetAVPack`'s one real call site, `0x821f5d14`:

```
821f5d14  bl 0x823d004c        ; XGetAVPack() -> r3
821f5d18  cmplwi cr6,r3,0x3
821f5d1c  beq cr6,0x821f5eac   ; skip setup
821f5d20  cmplwi cr6,r3,0x6
821f5d24  beq cr6,0x821f5eac   ; skip setup
821f5d28  cmplwi cr6,r3,0x8
821f5d2c  beq cr6,0x821f5eac   ; skip setup
821f5d30  cmplwi cr6,r3,0x4
821f5d34  beq cr6,0x821f5eac   ; skip setup
```

All four equality checks branch to the *same* target, and the return value
is never stored or read again afterward (the fall-through path proceeds
into a language-menu table setup this project's Gate 2 target needs to
reach). Any value outside `{0x3, 0x6, 0x8, 0x4}` is therefore behaviorally
identical at this one call site — unlike `XGetGameRegion` (r172), no
evidence here distinguishes between the remaining values.

## Value chosen

`0u` — the simplest value outside the four-item skip set, not asserted to
match any specific real AV-pack enum meaning (this project has no reachable
evidence pinning one), only to avoid the four values this XEX's own code
treats specially.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 157/157
(156/156 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. `XGetLanguage` (`0x821f5d9c`) remains the last unchecked import in this
   family — its result is read again and bounds-checked, unlike
   `XGetAVPack`, so a separate cycle implements and verifies it.
2. `VdGetCurrentDisplayInformation` struct+0x05 (r170) remains named but
   unimplemented.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
