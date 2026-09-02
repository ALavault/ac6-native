# AC6 retail NTSC-U/J — real fix: `XamTaskCloseHandle` returns success (r230)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. `ctest` 10/10 (`build/ntsc-uj/native/native-cmake`), pytest
209/209 (208 passed, 1 pre-existing skip).

## What was found

r198 deferred `XamTaskSchedule` and `XamTaskCloseHandle` together as a
pair, reasoning that a real fix for either needs a guest-callback
execution subsystem this project has never built. That reasoning holds for
`XamTaskSchedule` (it schedules a guest callback that is never actually
invoked), but this cycle traced `XamTaskCloseHandle`'s own single real
call site and found it needs no such subsystem.

`scripts/FindSymbolReferences.java` found one reference:
`0x82391e00 UNCONDITIONAL_CALL`, inside `Function_82391A40` (a large
save-content-scan/write function, part of the same territory as r202's
save/reload frontier). The call appears as
`func_0x823d09bc(auStack_4b0[0])`, immediately following a successful
`XamTaskSchedule` call (`func_0x823d09cc(...)`, gated on `-1 < iVar5`).
**The return value is discarded outright** — no check of any kind, not
even an implicit one via a following branch. This is the same
"ignored-return" shape already fixed for `KeLockL2`/`KeUnlockL2` (r197),
`IoDismountVolume` (r204), `XamVoiceClose` (r208), and
`XMsgCancelIORequest` (r215).

## Fix

`tools/materialize_native_import_stubs.py`: `XamTaskCloseHandle` now
returns `STATUS_SUCCESS` unconditionally, matching the established
ignored-return pattern. `XamTaskSchedule` itself remains deferred — this
fix does not change that, since it addresses only the handle-close half
of the pair r198 originally grouped together.

## Gates

- `python3 -m pytest tests/` — 209/209 (208 passed, 1 pre-existing skip;
  new test `test_xam_task_close_handle_returns_success`).
- `python3 tools/build.py --target ntsc-uj --profile native` — clean.
- `ctest` (`build/ntsc-uj/native/native-cmake`) — 10/10.
- `git status` after `ctest` (not before): only the two edited source
  files changed.

## Next

1. Continue the offline-import sweep per r218's catalog — 81 generic
   imports remain after this fix.
2. `VdGetSystemCommandBuffer`/`VdPersistDisplay` have real callers but
   remain out of scope: renderer-policy territory (the native Vulkan
   backend is deliberately fail-closed pending oracle-free microcode
   qualification, per this project's standing policy).
3. `__C_specific_handler` confirmed to have zero references anywhere in
   this XEX (checked this cycle) — SEH's dispatcher is never actually
   invoked in this build, consistent with r202's `RtlRaiseException`/
   `RtlUnwind`/`RtlCaptureContext` finding.
4. Remaining untraced candidates: the networking cluster (~29 imports),
   `_vsnprintf`/`sprintf` (varargs printf engine, out of a bounded
   cycle's scope), `XeKeysConsolePrivateKeySign`/
   `XeKeysConsoleSignatureVerification` (permanently out of scope per
   r202).
5. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
