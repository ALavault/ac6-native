# AC6 retail NTSC-U/J — real fix: `KeDelayExecutionThread` actually sleeps (r194)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test (and one pre-existing test's fallback example
retargeted, see below). Verified via a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 10/10) and the full retail-native pytest
suite (181/181, up from 180/180).

## What was found

`NTSTATUS KeDelayExecutionThread(KPROCESSOR_MODE WaitMode, BOOLEAN
Alertable, PLARGE_INTEGER Interval)` is a standard, documented NT kernel
API. This XEX has a single real call site (`0x821f74e8`), inside a wrapper
function (`Function_821F7498`, real range `[0x821f7498, 0x821f7523]`) that
converts a millisecond count in its own argument into a real, negative
(relative) 100ns `LARGE_INTEGER` before the call (`mulli r11,r11,-0x2710`
— milliseconds × -10000), or a fixed `0x8000000000000000` sentinel for an
infinite wait when passed `-1` ms.

That wrapper normalizes every possible return status down to just `0` or
`0xC0` (`STATUS_USER_APC`) — and the generic offline no-op's
`kOfflineStatus` (`0xC00000BB`) already happened to fall into the `0`
branch by accident, since it matches neither compared constant
(`0x101`/`0xC0`). So the *status* was never actually wrong at this call
site. The real bug is *timing*: the no-op returns instantly, collapsing
what this XEX's own code computed as a real millisecond delay into zero
elapsed time — a genuine pacing/timing correctness gap for a primitive
this project's own r179/r185 work already established real wall-clock
progression matters for.

## Fix

Reads the real 64-bit `Interval` from guest memory (`PPC_LOAD_U64`); if
negative (relative wait, the only form this XEX's own real call site
produces), converts the magnitude from 100ns units to a `std::chrono`
duration and calls `std::this_thread::sleep_for` on it, then returns
`STATUS_SUCCESS`. A positive (absolute) `Interval` is left unhandled
rather than guessed, since no real call site in this XEX uses that form.

One pre-existing test,
`test_generic_fallback_is_traceable_and_still_returns_offline_status`, had
used `KeDelayExecutionThread` as its example of the generic-fallback
shape; retargeted to `NtCancelTimer` (still generic) rather than deleted,
since the test's own purpose (assert the fallback path itself still
works) is independent of which import demonstrates it.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
181/181 (180/180 before this cycle, +1 new test,
`test_ke_delay_execution_thread_actually_sleeps`; one pre-existing test
retargeted, not added or removed). `git status` unchanged apart from the
intended change set and the same pre-existing, unrelated dirty state noted
by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r193's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r194).
2. If a future call site is found passing a positive (absolute) `Interval`
   to `KeDelayExecutionThread`, implement that form only once traced —
   this cycle deliberately left it unhandled rather than guess a
   plausible-looking conversion.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
