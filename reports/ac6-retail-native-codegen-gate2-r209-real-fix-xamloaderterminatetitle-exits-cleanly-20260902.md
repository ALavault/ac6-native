# AC6 retail NTSC-U/J — real fix: `XamLoaderTerminateTitle` exits cleanly (r209)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(195/195, up from 194/194).

## What was found

`VOID XamLoaderTerminateTitle(VOID)` is a documented, parameterless XAM
API that tears down the running title and never returns. This XEX has 2
real call sites (`0x821f5f18`, `0x821f608c`). The second is conclusive:
the instruction immediately after the call (`0x821f6090`) is a
**different function's own prologue** (`mfspr r12,LR`) — the compiler
emitted no epilogue at all after this call site, meaning it never
expected control to return there. This is the same "never returns" class
r193 already fixed for `KeBugCheck`/`KeBugCheckEx`, but a normal
title-exit path, not a fault.

## Fix

Calls `std::exit(0)` — a clean process exit rather than `std::abort()`,
matching the real semantics (a normal, requested title termination, not
a crash).

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
195/195 (194/194 before this cycle, +1 new test,
`test_xam_loader_terminate_title_exits_cleanly`). `git status` unchanged
apart from the intended change set and the same pre-existing, unrelated
dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r208's reports, not touched by
this cycle.

## Addendum: `XamGetExecutionId` (deferred in r206) has wider reach than scoped there

While investigating `XamUserReadProfileSettings` (4 real call sites) this
cycle, found that all of them are guarded by a call to the same wrapper
function (`0x821f7668`) r206's `XamGetExecutionId` investigation already
traced: if the wrapper's own comparison check is invoked with a nonzero
argument, `kOfflineStatus`'s negative value makes the wrapper's internal
`XamGetExecutionId` call fail unconditionally, which in turn makes the
*caller* skip the `XamUserReadProfileSettings` call entirely. This means
r206's deferral of `XamGetExecutionId` (an unconfirmed struct-field
comparison) is not an isolated, narrow gap — it gates at least 4 more
real call sites through this one shared wrapper. Not fixed this cycle
(the struct field this comparison reads is still not independently
confirmed, and this cycle did not trace far enough up the call chain to
learn what value the wrapper's own check argument carries at each of
these 4 sites) — named here so a future cycle knows the deferred item's
real blast radius before deciding whether to invest in tracing it
further.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r209).
2. If `XamGetExecutionId` is revisited, trace the wrapper's own check
   argument at `XamUserReadProfileSettings`'s 4 real call sites
   (`0x8220775c`, `0x821fe320`, `0x821fe390`, `0x82206b88`) before
   attempting a fix — this cycle established the connection but not the
   value.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
