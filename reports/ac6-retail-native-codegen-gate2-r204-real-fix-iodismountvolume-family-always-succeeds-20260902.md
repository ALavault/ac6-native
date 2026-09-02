# AC6 retail NTSC-U/J — real fix: `IoDismountVolume`/`IoDismountVolumeByFileHandle` always succeed (r204)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(190/190, up from 189/189).

## What was found

`IoDismountVolume` (2 real call sites: `0x82391984`, `0x823919b8`, both
inside `Function_823917F8` — the same function r198's
`XamTaskShouldExit` fix already lives in) and
`IoDismountVolumeByFileHandle` (1 real call site: `0x82392d6c`) all
share the same shape: the caller discards the return value entirely,
with no check at all.

`IoDismountVolumeByFileHandle`'s call site is notable: it is the
unconditional cleanup tail of r202's traced save-write function
(`Function_82392878`/the function starting `0x82392978`) — reached from
both the success path (falling through the write loop) and the failure
path (`blt` jumping to the same cleanup label) — a real "always dismount
on the way out" step, not gated behind a successful write.

## Fix

Both now return `STATUS_SUCCESS` unconditionally, with no other side
effect — same class of fix as r197's `KeLockL2`/`KeUnlockL2`/
`KiApcNormalRoutineNop` (the generic fallback's only actual defect here
was diagnostic trace noise, not observable behavior, since nothing reads
the return value).

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
190/190 (189/189 before this cycle, +1 new test,
`test_io_dismount_volume_family_always_succeeds`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r203's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r204).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
