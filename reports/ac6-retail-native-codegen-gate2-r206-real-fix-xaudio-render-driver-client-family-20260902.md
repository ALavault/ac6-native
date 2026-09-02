# AC6 retail NTSC-U/J — real fix: `XAudioRegisterRenderDriverClient`/`Unregister`/`SubmitRenderDriverFrame` (r206)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(192/192, up from 191/191).

## What was found

Traced all three real call sites inside `Function_823A6620`
(`[0x823a6620, 0x823a6687]`, the audio-driver init/teardown wrapper) and
`Function_823A6878` (`[0x823a6878, ...]`, the frame-submit wrapper):

- `XAudioUnregisterRenderDriverClient` (real call site `0x823a664c`):
  return checked **signed** (`blt` error) — `kOfflineStatus` (negative)
  meant this call site's re-init path *always* aborted before ever
  reaching the register call below it, a real init-order blocker.
- `XAudioRegisterRenderDriverClient` (real call site `0x823a667c`,
  immediately after): `r4` is an output handle pointer — confirmed by
  reading back the exact same storage slot (`r31+0x18`) the
  Unregister call above reads its own handle argument from. Its own
  return value is not checked at all in this wrapper.
- `XAudioSubmitRenderDriverFrame` (real call site `0x823a68c0`, in the
  separate frame-submit wrapper): return value discarded entirely, no
  check.

This project has no real audio-render-driver pipeline (no host audio
device submission), and `XAudioSubmitRenderDriverFrame`'s own ignored
return confirms nothing downstream needs one — the whole triplet is pure
handle bookkeeping plus a fire-and-forget frame handoff, not audio
signal processing this cycle would need to fabricate.

## Fix

`XAudioRegisterRenderDriverClient` allocates a handle from the same
`g_next_handle` counter `NtCreateTimer`/`NtCreateMutant` already use and
writes it through the output pointer, returning success.
`XAudioUnregisterRenderDriverClient` returns success unconditionally
(matching the handle's no-op-release precedent used elsewhere in this
file). `XAudioSubmitRenderDriverFrame` accepts and discards the frame,
returning success — honest given no real audio output device exists to
render it to, not a fabricated result (its own real call site never
reads the return value either way).

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
192/192 (191/191 before this cycle, +1 new test,
`test_xaudio_render_driver_client_family`). `git status` unchanged apart
from the intended change set and the same pre-existing, unrelated dirty
state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r205's reports, not touched by
this cycle.

## Also checked, no safe fix: `XamGetExecutionId`, `XamShowMessageBoxUIEx`, `XamShowSigninUI`-family

Three more candidates were traced this cycle and deliberately not fixed:

- `XamGetExecutionId` (1 real call site, `0x821f7690`): the caller
  compares a 16-bit field at offset `+0xC` of the returned
  `XAM_EXECUTION_INFO`-shaped struct against a caller-supplied value.
  This project has not independently confirmed that field's real
  meaning at this offset; writing a value there without that
  confirmation would be exactly the kind of guess this project's
  evidence discipline forbids.
- `XamShowMessageBoxUIEx` (1 real call site, `0x821f5c30`): follows the
  standard XAM async-UI contract (`ERROR_IO_PENDING`/997, then an
  overlapped-wait helper reads the eventual button choice). A correct
  fix needs either a real overlapped-wait subsystem or a confidently
  derived default button-result register — neither was established this
  cycle.
- `XamShowSigninUI` and 6 sibling `XamShow*` dialogs (`XamShowGamerCardUIForXUID`,
  `XamShowFriendsUI`, `XamShowMarketplaceUI`, `XamShowPlayerReviewUI`,
  `XamShowDirtyDiscErrorUI`, `XamShowDeviceSelectorUI`): each routes
  through a one-instruction tail-jump trampoline in a small jump-table
  region near `0x821f4668`-`0x821f4698`. The trampolines are pure glue —
  determining a safe default return requires tracing each trampoline's
  own caller individually, not done this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r206).
2. `XamGetExecutionId`'s struct field and the `XamShow*` trampoline
   family are both named, scoped candidates for a future cycle with
   more tracing budget.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
