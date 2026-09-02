# AC6 retail NTSC-U/J — real fix: `VdSetDisplayMode` always succeeds (r217)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(203/203, up from 202/202).

## What was found

`VdSetDisplayMode`'s single real call site (`0x821f075c`, inside
`Function_821F03B0` — the same function r198 already traced for
`VdGetSystemCommandBuffer`) discards the return value outright, falling
straight into the next call's own setup (`VdGetCurrentDisplayInformation`,
already fixed at r175) with no check at all.

## Fix

Returns `STATUS_SUCCESS`/`0` unconditionally — same "native renderer owns
the Vd lifecycle" precedent as `VdRetrainEDRAM` and its siblings.

## Also checked, no fix: `VdPersistDisplay`

Its single real call site (`0x821f09d8`) **does** check the return
value and, on a nonzero result, feeds a locally-built command buffer
(constructed just before the call) into a further display-submission
call. Unlike `VdSetDisplayMode`, this connects directly to real
frame/display-buffer state this project's native Vulkan renderer
deliberately keeps fail-closed pending an oracle it has never used
(project policy). Fixing this would risk asserting graphics behavior
this cycle cannot verify against a real renderer. Left as the generic
offline no-op.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
203/203 (202/202 before this cycle, +1 new test,
`test_vd_set_display_mode_always_succeeds`). `git status` unchanged
apart from the intended change set and the same pre-existing, unrelated
dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r216's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r217). The remaining generic-fallback list (~87
   imports) is now overwhelmingly concentrated in a handful of large,
   already-documented buckets: `NetDll_*` networking (~29 imports, no
   real network stack), SEH (`RtlRaiseException`/`RtlUnwind`/
   `RtlCaptureContext`), the save-write path (`NtWriteFile`/
   `NtDeviceIoControlFile`/`NtSetInformationFile`/`NtQueryInformationFile`,
   r202), the `XMsg*` message-dispatch family (r203), `sprintf`/
   `_vsnprintf` (varargs engine), the `XamGetExecutionId`-gated
   identity/profile cluster (r211), and the untraced `XamShow*`/
   `XamContent*` UI-dialog trampolines (r206/r212). Individual small
   wins outside those buckets are now sparse.
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
