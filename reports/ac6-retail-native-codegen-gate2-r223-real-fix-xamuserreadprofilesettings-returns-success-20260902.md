# AC6 retail NTSC-U/J — real fix: `XamUserReadProfileSettings` returns success (r223)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(205/205, up from 204/204).

## What was found

r219 found `XamUserReadProfileSettings`'s own caller (`Function_821CE6C0`)
checks for specific return codes (`0x7a`/122, `0x3e5`/997) rather than
any-nonzero, and flagged this as needing the same async-completion care
r220/r221/r222 worked out for `XamShowMessageBoxUIEx`. This cycle traced
both of the caller's own "did not match that specific code" branch
targets (`0x821ce6dc`, `0x821ce8c8`): **neither is an error path**. One
is simply the caller's own clean `return 0` (success); the other sets an
internal flag byte and returns `1` (also success). So any return value
other than those two exact codes is *already* handled safely by this
caller — the caller was never distinguishing "success vs. failure" at
those branches, only "which of two known-good outcomes."

The traced real call site (`0x821fe2f0`, inside `Function_821FE2C8`)
passes `dwNumSettingIds=0`/`pdwSettingIds=NULL` — a degenerate call
requesting zero settings, so `STATUS_SUCCESS` is the honest, direct
match, not a guess standing in for real data.

## Fix

Returns `STATUS_SUCCESS`/`0`. This import's real signature beyond the
first 4 parameters (`dwTitleId`, `dwUserIndex`, `dwNumSettingIds`,
`pdwSettingIds`) was not confirmed closely enough this cycle to know
with confidence which register holds `pcbResults`/`pResults` for this
specific SDK build (unlike `XamShowMessageBoxUIEx`, which matched the
documented 9-parameter signature cleanly, this import showed 8
register slots plus a stack-passed 9th — two more than the commonly
documented 7-parameter form) — so only the return status is fixed,
matching this project's own discipline of not writing through an
unconfirmed output-parameter register.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
205/205 (204/204 before this cycle, +1 new test,
`test_xam_user_read_profile_settings_returns_success`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r222's reports, not touched by
this cycle.

## Next

1. If a future cycle needs the full struct-fill (a real, non-degenerate
   settings read), first confirm which registers hold
   `pcbResults`/`pResults`/`pOverlapped` for this SDK build's real
   >7-parameter signature — this cycle only confirmed the first 4 and
   the safety of the return-status-only fix.
2. Continue the broader offline-import sweep per r218's bucket catalog.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
