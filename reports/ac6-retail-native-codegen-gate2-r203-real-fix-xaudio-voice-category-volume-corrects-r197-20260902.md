# AC6 retail NTSC-U/J — real fix: `XAudioGetVoiceCategoryVolume`/`VolumeChangeMask`; corrects r197 (r203)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(189/189, up from 188/188).

## Correction to r197: `0x823cfe4c` is `XMsgStartIORequest`, not an internal telemetry function

While investigating `XMsgStartIORequest` this cycle (17 real call sites —
the highest count in the offline-import sweep so far), its real address
was confirmed via `scripts/FindSymbolReferences.java` to be
**`0x823cfe4c`**. r197's report explicitly described this same address as
"an internal (non-imported) function... alongside constants that look
like a fixed telemetry/event-tracing record (event id `0xfb`...)" when
investigating `XamSessionCreateHandle`/`XamSessionRefObjByHandle`'s
sibling wrapper family. **Both of those claims were wrong**: `0x823cfe4c`
is the real, documented XAM import `XMsgStartIORequest`, and the `0xfb`
constant r197 called an "event id" is that call's `MessageType` argument
(`r3`), not a telemetry event.

This means r197's whole deferred family
(`XamSessionCreateHandle`/`XamSessionRefObjByHandle` and the surrounding
wrapper functions near `0x821fd3e8`-`0x821fd9xx`) is not a telemetry
subsystem at all — it is real traffic through the same `XMsgStartIORequest`
transport many other XAM APIs route through internally. This is a *larger*
finding than r197 realized, not a smaller one: fixing any of it correctly
now depends on understanding `XMsgStartIORequest`'s message-dispatch
contract, which this cycle did not attempt (see below) — r197's practical
conclusion (leave the family as the generic no-op, deferred) still holds,
but for the corrected reason. Named here per this project's own evidence
discipline (correct predecessors, including this session's own prior
cycles, by name and cycle number).

## What was found: `XMsgStartIORequest`/`XMsgInProcessCall` family, deferred

`XMsgStartIORequest` (17 real call sites) and `XMsgInProcessCall` (6 real
call sites) are the real cross-subsystem message-dispatch transport many
higher-level XAM APIs use internally (confirmed: it is what r197's
`XamSession*` wrapper family calls, and it is also called from at least
two other function clusters near `0x821f42xx`/`0x821f43xx` and
`0x82210dbc`). A correct fix needs to enumerate and understand the
distinct message types passed across at least 17+ call sites (`0xfb` was
only one of them, traced in r197's now-corrected investigation) — a
substantially larger, multi-cycle undertaking, not a bounded fix. Left as
the generic offline no-op; not attempted further this cycle.

## Real fix: `XAudioGetVoiceCategoryVolumeChangeMask`/`XAudioGetVoiceCategoryVolume`

A smaller, cleanly bounded pair found in the same pass. Both real call
sites (`0x823ad1fc`, `0x823ad22c`, both inside `Function_823AD1C0`) are
confirmed via exact-symbol search — an earlier naive substring search on
"XAudioGetVoiceCategoryVolume" also matched
"XAudioGetVoiceCategoryVolumeChangeMask" and would have double-counted,
the same class of gotcha r193 already hit for `KeBugCheck`/`KeBugCheckEx`.

`HRESULT XAudioGetVoiceCategoryVolumeChangeMask(DWORD Handle, PDWORD
pChangeMask)`: the real caller loops over 2 volume categories testing
bits of `*pChangeMask` to decide whether to re-query each one via
`XAudioGetVoiceCategoryVolume(DWORD CategoryIndex, float* pVolume)`. This
project has no real volume-mixer subsystem to report changes from, so
"nothing changed" (`*pChangeMask = 0`) is the honest default — and, as a
direct structural consequence, the per-category re-query it gates almost
never fires, which is correct behavior absent a real change source, not a
workaround. `XAudioGetVoiceCategoryVolume` reports full volume (`1.0f`)
when it is queried.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
189/189 (188/188 before this cycle, +1 new test,
`test_xaudio_voice_category_volume_reports_no_change_full_volume`). `git
status` unchanged apart from the intended change set and the same
pre-existing, unrelated dirty state noted by every prior cycle in this
series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r202's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r203).
2. If `XMsgStartIORequest`'s message-dispatch contract is ever tackled,
   start from the corrected understanding here: it is real IPC-style
   transport, not telemetry, with at least 17 real call sites and
   multiple distinct message types to enumerate.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
