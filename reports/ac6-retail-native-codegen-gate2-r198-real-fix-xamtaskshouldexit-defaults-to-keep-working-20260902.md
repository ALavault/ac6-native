# AC6 retail NTSC-U/J — real fix: `XamTaskShouldExit` defaults to "keep working" (r198)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(185/185, up from 184/184).

## What was found

`BOOLEAN XamTaskShouldExit(VOID)` is a documented, parameterless XAM API a
worker spawned via `XamTaskSchedule` polls to check whether it should
abort early. This XEX's single real call site (`0x82391950`, inside
`Function_823917F8`) passes no argument, matching the real signature. The
generic offline no-op's `kOfflineStatus` (nonzero) read as "should exit
== true", so this call site's worker loop aborted immediately on every
pass — before doing any of its scheduled work.

## Fix

Returns `0` (`FALSE`, "keep working") unconditionally — the honest default
absent any real exit-signaling mechanism, since this project has no
`XamTaskSchedule`-driven worker execution yet (see below).

## Also checked, no fix: `XamTaskSchedule`/`XamTaskCloseHandle`

Traced the real call site of `XamTaskSchedule` (`0x82391df0`) and the
`XamTaskCloseHandle` call immediately after it (`0x82391e00`), both
inside the same function (`Function_82391A40`, real range
`[0x82391a40, 0x82391e7f]`). The real contract is `DWORD
XamTaskSchedule(XAMTASK_ROUTINE Routine, PVOID Context, LPOVERLAPPED
pOverlapped, PHANDLE pHandle)`: schedules `Routine(Context)` to run
asynchronously and, on success, the real code immediately calls
`XamTaskCloseHandle` on the handle it just got back (fire-and-forget —
schedule the work, release the local reference, let it run in the
background). A correct fix needs to actually invoke a guest function
pointer (`Routine`) with a guest context, which means calling back into
the recompiled PPC code from wherever this project would run that work
— a real async-execution subsystem, not a one-line contract fix. Not
attempted this cycle; `XamTaskCloseHandle` alone would be unreachable
dead code without `XamTaskSchedule` fixed first, so it was left alongside
it rather than fixed in isolation.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
185/185 (184/184 before this cycle, +1 new test,
`test_xam_task_should_exit_defaults_to_keep_working`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r197's reports, not touched by
this cycle.

## Also checked, no fix: `VdGetSystemCommandBuffer`

Traced its single real call site (`0x821f061c`, inside
`Function_821F03B0`): writes two output DWORDs consumed by later code in
the same function. This is graphics-subsystem plumbing (a distinct
"system" command buffer from the ring buffer this project's
`NativeGuestVdService` already tracks), and this project's policy keeps
native Vulkan rendering work fail-closed pending an oracle it has never
used — implementing this correctly risks either guessing values with no
renderer behind them to validate against, or scope creep into the
renderer frontier the user has explicitly deferred. Not attempted.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r198).
2. `XamTaskSchedule`/`XamTaskCloseHandle` are a real, scoped follow-up
   candidate once (or if) this project builds any guest-callback
   execution mechanism for another reason — implementing one solely for
   this pair would be disproportionate to what it's worth on its own.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
