# AC6 retail NTSC-U/J — real fix: `XamVoiceHeadsetPresent` reports absent (r207)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(193/193, up from 192/192).

## What was found

`BOOL XamVoiceHeadsetPresent(HANDLE hVoice)` is a plain boolean, not an
`NTSTATUS`. This XEX's real call site (`0x82206900`, inside
`Function_822068E0`) compares the result directly against zero
(`cmpwi cr6,r3,0x0`) with no signed status check — confirming the plain
boolean contract. `kOfflineStatus` (`0xC00000BB`) is nonzero, so it read
as `TRUE` ("headset present") on every call — this project has no real
microphone/headset device, so this was a status code misreported as
"headset connected," the same class of bug as r200's `XNotifyGetNext`
(a status masquerading as a different-typed value).

The caller wraps this in its own change-detection logic (comparing the
current reading against a stored previous one to decide whether presence
"changed"); traced through that logic, a constant `FALSE` is stable —
once the first reading settles, it never spuriously reports a change.

## Fix

Returns `0` (`FALSE`, no headset) unconditionally — the honest report
given this project's SDL2 input backend (r180) has no voice/microphone
device modeled.

## Also checked, no fix: `XamVoiceCreate`

Traced its real call site (`0x82207620`, inside `Function_822075F8`):
the caller checks the result **signed** (`blt` error), so
`kOfflineStatus`'s current negative value already produces a clean
failure and skips all subsequent voice-channel setup — which, absent any
real microphone input this project could actually feed into a voice
channel, is arguably the honest outcome already, not a bug the generic
no-op happens to get wrong. Left unchanged rather than force a "succeeds
with a fake handle" fix whose correctness this cycle could not confirm
is actually better than the current clean failure.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
193/193 (192/192 before this cycle, +1 new test,
`test_xam_voice_headset_present_reports_absent`). `git status` unchanged
apart from the intended change set and the same pre-existing, unrelated
dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r206's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r207).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
