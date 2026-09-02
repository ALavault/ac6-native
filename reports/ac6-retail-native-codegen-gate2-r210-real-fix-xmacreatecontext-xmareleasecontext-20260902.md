# AC6 retail NTSC-U/J — real fix: `XMACreateContext`/`XMAReleaseContext` (r210)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(196/196, up from 195/195).

## What was found

`XMACreateContext` (real call site `0x823aec8c`, inside
`Function_823AEC38`): `r3` is an output handle pointer, confirmed by
reading the same storage slot back immediately after success to feed a
follow-up query call. Its return is checked **signed** (`blt` error), so
`kOfflineStatus` (negative) currently blocks all XMA (compressed-audio
decode) context setup unconditionally — a real init-order blocker for
whatever music/audio streaming path uses this codec context, the same
shape as r206's `XAudioUnregisterRenderDriverClient` finding.

`XMAReleaseContext` (real call site `0x823ae37c`, inside
`Function_823AE340`): return value discarded outright, no check at all.

## Fix

`XMACreateContext` allocates a handle from the same `g_next_handle`
counter `NtCreateTimer`/`XAudioRegisterRenderDriverClient` (r206) already
use, writes it through the output pointer, and succeeds.
`XMAReleaseContext` returns success unconditionally — same
ignored-return precedent as r197's `KeLockL2`/`KeUnlockL2`.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
196/196 (195/195 before this cycle, +1 new test,
`test_xma_context_family`). `git status` unchanged apart from the
intended change set and the same pre-existing, unrelated dirty state
noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r209's reports, not touched by
this cycle.

## Also checked, no fix: `XamVoiceSubmitPacket`

Both real call sites depend on a voice handle only `XamVoiceCreate`
produces, and r207 deliberately left `XamVoiceCreate`'s current (honest)
failure unchanged. Fixing `XamVoiceSubmitPacket` alone is currently
inert — the same "unreachable without its own prerequisite" reasoning as
r198's `XamTaskCloseHandle`. Not attempted.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r210).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
