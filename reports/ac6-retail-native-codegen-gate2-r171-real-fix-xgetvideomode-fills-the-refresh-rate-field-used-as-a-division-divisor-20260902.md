# AC6 retail NTSC-U/J — real fix: `XGetVideoMode` fills the refresh-rate field used as a division divisor (r171)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(155/155, up from 154/154).

## Why this check was worth running

r170 closed the named `VdQueryVideoMode`-family sweep with no further named
gap. Rather than guess at the next candidate, this cycle statically checked
the small set of `X*`-prefixed platform-config imports still on the generic
offline-import fallback (`XGetVideoMode`, `XGetGameRegion`, `XGetAVPack`,
`XGetLanguage`) — the same shape-risk category as the Vd family, since these
are also thin platform-query APIs.

## What was found

`XGetVideoMode`'s one real call site (`sub_82339780` at `0x82339794`-
`0x82339798`, struct at `[r1+0x60]`):

```
82339794  addi r3,r1,0x60
82339798  bl 0x823d095c        ; XGetVideoMode(&struct)
823397a0  lfs f0,0x74(r1)      ; struct+0x14, float
823397a4  fmr f31,f0
823397a8  lfs f13,0x7fc(r11)   ; a sentinel constant
823397ac  fcmpu cr6,f0,f13
823397b0  bne cr6,0x823397bc
823397b4  lfs f31,0xb04(r11)   ; ... override with a different constant if equal
823397bc  ...
823397f0  fdivs f1,f31,f0      ; struct+0x14 used as a division DIVISOR
```

`struct+0x14` is the **same offset** r169 confirmed as a real,
arithmetic-used refresh-rate-shaped float field in `VdQueryVideoMode`'s own
output struct — real hardware documents `XGetVideoMode` as the XAM-level
wrapper around `VdQueryVideoMode`, sharing the same `XVIDEO_MODE` struct,
and this is now independent, real-disassembly corroboration of that from a
second call site in this XEX, not an assumption borrowed from any
independent reimplementation.

Left unimplemented, this divisor is whatever garbage occupies this XEX's
own stack slot at the time of the call — a real risk of an unintended
near-zero divide producing `Inf`/`NaN` that propagates into further guest
floating-point math, not merely an inert unread field.

Only `struct+0x14` is read at this one call site; `+0x00`/`+0x04`/`+0x08`
are not asserted here without their own reading evidence at this specific
call site, matching r169's own discipline of implementing only offsets
this XEX's own disassembly actually reads.

## Value chosen

`60.0f` (`0x42700000`) — the same standard NTSC refresh-rate default r169
used for `VdQueryVideoMode`, not invented for this fix, and consistent with
the shared struct/field identity established above.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 155/155
(154/154 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. `XGetGameRegion` (3 real call sites, e.g. `0x821babdc`) is a stronger,
   not-yet-implemented candidate: its return value is stored into a table
   slot and immediately read back and compared against several specific
   constants (`0x1ff`, `0x101`, `0x102`, `0x1fc`) that gate real control
   flow — plausibly a genuine region-detection bug, but determining the
   correct region-code value requires reading all three call sites'
   downstream branches, not done this cycle.
2. `XGetAVPack` (one call site, `0x821f5d14`) and `XGetLanguage` (one call
   site, `0x821f5d9c`) are not yet checked for the same category of gap.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
