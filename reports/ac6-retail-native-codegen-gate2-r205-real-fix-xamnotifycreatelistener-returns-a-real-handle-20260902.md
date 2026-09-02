# AC6 retail NTSC-U/J — real fix: `XamNotifyCreateListener` returns a real handle (r205)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(191/191, up from 190/190).

## What was found

`HANDLE XamNotifyCreateListener(ULONGLONG qwAreas)` returns a `HANDLE`,
not an `NTSTATUS`. This XEX's real call site (`0x82204f08`, inside
`Function_82204DC8`, the same retry-loop wrapper r197's
`NtDuplicateObject` investigation traced) checks the result with
`cmplwi r3,0x0; beq <retry-path>` — a zero/invalid handle triggers a
retry, so any nonzero value reads as "handle acquired." `kOfflineStatus`
(`0xC00000BB`) is nonzero, so the generic offline no-op was silently
handing back a status code disguised as a valid handle.

This is not currently an observable behavior bug for this specific
consumer: r200's `XNotifyGetNext` fix already ignores its own handle
argument entirely (it unconditionally reports "no notification
pending"), so a garbage handle value flowing into it is harmless in
practice. It is still dishonest to report a status code as a handle, and
a future consumer of this same handle (or a closer trace of this call
site's own further use of the value) could depend on it being a real,
distinguishable identifier rather than an opaque constant that happens
not to be zero.

## Fix

Returns a real handle allocated from `g_next_handle`, the same counter
`NtCreateTimer`/`NtCreateMutant` already use for handle-shaped imports
with no real backing kernel object.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
191/191 (190/190 before this cycle, +1 new test,
`test_xam_notify_create_listener_returns_a_real_handle`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r204's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r205).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
