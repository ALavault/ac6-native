# AC6 retail NTSC-U/J — real fix: `XNotifyGetNext` reports no notification pending (r200)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(187/187, up from 186/186).

## What was found

`BOOL XNotifyGetNext(HANDLE hNotification, DWORD dwMsgFilter, PDWORD
pdwId, PULARGE_INTEGER pParam)` is a standard, documented XAM API this
XEX calls from 4 real call sites (`0x82165868`, `0x8215ca64`,
`0x821ce1c0`, `0x82204590`). Traced one directly (`0x82165868`, inside
`Function_82165848`): the contract is confirmed by `cmpwi cr6,r3,0x0; beq
... skip` — zero means "no notification pending," and only a nonzero
return causes the caller to read `*pdwId` (compared against a specific
expected id, `0x11`) and act on it.

`kOfflineStatus` is nonzero, so this XEX's own real code was treating
every single call as "a notification is pending" and reading a
notification id from a stack slot this project's generic stub never
wrote — driving a real conditional branch off uninitialized memory on
every call, not a cosmetic status-shape mismatch. This project has no
real system-notification queue to drain (sign-in changes, UI events,
etc.), so "no notification pending" is the correct, honest report, not a
guess standing in for missing data.

## Fix

`XNotifyGetNext` returns `0` (`FALSE`) unconditionally — the same
"honest default absent a real signal source" class of fix as r198's
`XamTaskShouldExit`. `XNotifyPositionUI` (cosmetic: repositions the
system notification popup; its single real call site, `0x821ce0ec`,
discards the return value outright) is fixed alongside it as the same
trivial no-op family.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
187/187 (186/186 before this cycle, +1 new test,
`test_xnotify_family_reports_no_notification_pending`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r199's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r200).
2. If a real system-notification source (sign-in changes, achievements,
   controller events distinct from r180's input backend) is ever
   modeled, `XNotifyGetNext` is the single integration point to revisit.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
