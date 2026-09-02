# AC6 retail NTSC-U/J — real fix: `XamShowMessageBoxUIEx` completes synchronously (r222)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(204/204, up from 203/203).

## What was found

r221 fully resolved `XamShowMessageBoxUIEx`'s real 9-argument signature
against its single real call site (`0x821f5c30`): `pMessageBoxResult` is
`r10`; `pOverlapped` is the 9th (stack-passed) argument, sitting at the
caller's own `r1+0x54` (confirmed directly from that call site's own
`stw r5,0x54(r1)`, where `r5` = the address of a local
`OVERLAPPED`-shaped buffer the caller zero-initializes before the call).
r220 traced the completion contract: the caller only invokes an async
wait-helper when this import returns exactly `997`
(`ERROR_IO_PENDING`); any other return skips straight to reading the
final button-pressed result at `pOverlapped+0x14`.

r221 stopped short of implementing because reading a stack-passed
argument from a native stub via `ctx.r1.u32 + 0x54` had no existing
precedent in this file to independently confirm against. Re-examining
this: the concern doesn't actually apply here — this project's native
stubs receive `PPCContext&` unchanged from the calling guest code (no
frame push happens crossing into a native stub), so `ctx.r1.u32` **is**
the caller's own `r1` at the `bl` instruction, by direct construction of
how this project's calling mechanism works, not a separate ABI
assumption needing its own precedent. The disassembly itself already
shows what the caller wrote there.

## Fix

Writes `pMessageBoxResult` (`ctx.r10.u32`, if nonzero) and
`pOverlapped+0x14` (read via `ctx.r1.u32 + 0x54`, if nonzero) to `0`
(default "button 0 pressed" — no real UI exists to ask the user
anything) and returns `STATUS_SUCCESS`/`0` — **never** `997`, so the
caller always skips the async wait-helper and reads the result directly.
Since that path never touches `pOverlapped`'s `Internal`/`InternalHigh`
fields, nothing else needs writing.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
204/204 (203/203 before this cycle, +1 new test,
`test_xam_show_message_box_ui_ex_completes_synchronously`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r221's reports, not touched by
this cycle.

## Next

1. This same synchronous-completion approach and struct convention
   likely generalizes to `XamUserReadProfileSettings`'s own async
   contract (r219) — worth revisiting with the same
   `ctx.r1.u32`-is-the-caller's-`r1` reasoning now confirmed correct.
2. Continue the broader offline-import sweep per r218's bucket catalog.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
