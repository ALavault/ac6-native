# AC6 retail NTSC-U/J — real fix: `XMsgCancelIORequest` always succeeds (r215)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(201/201, up from 200/200).

## What was found

`XMsgCancelIORequest` cancels an in-flight `XMsg` transport request (the
same family r203 traced `XMsgStartIORequest` through and deferred). All
3 real call sites (`0x821f47b8`, `0x82206efc`, `0x822075c0`) were checked
individually this cycle: each discards the return value outright,
falling straight through with no check at all.

## Fix

Returns `STATUS_SUCCESS`/`0` unconditionally — same class of fix as
r197's `KeLockL2`/`KeUnlockL2`. Since this project's `XMsg` transport
itself is deferred (r203), there is never a real in-flight request for
this import to cancel; unconditional success is honest regardless of
whether the underlying transport is modeled.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
201/201 (200/200 before this cycle, +1 new test,
`test_xmsg_cancel_io_request_always_succeeds`). `git status` unchanged
apart from the intended change set and the same pre-existing, unrelated
dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r214's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r215).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
