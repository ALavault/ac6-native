# AC6 retail NTSC-U/J — real fix: `XamVoiceClose` always succeeds (r208)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(194/194, up from 193/193).

## What was found

`VOID XamVoiceClose(HANDLE hVoice)` has 3 real call sites
(`0x82207528`, `0x82207694`, `0x82206fa0`, across
`Function_82207500`/`Function_822075F8`/`Function_82206EE0`). All three
were checked individually this cycle: each discards the return value
outright, falling straight through to the next instruction with no
`cmpwi`/branch on it at all.

## Fix

Returns `STATUS_SUCCESS`/`0` unconditionally, with no other side effect —
same class of fix as r197's `KeLockL2`/`KeUnlockL2`/
`KiApcNormalRoutineNop` and r204's `IoDismountVolume` family: the generic
fallback's only actual defect here was diagnostic trace noise, not
observable behavior.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
194/194 (193/193 before this cycle, +1 new test,
`test_xam_voice_close_always_succeeds`). `git status` unchanged apart
from the intended change set and the same pre-existing, unrelated dirty
state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r207's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r208).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
