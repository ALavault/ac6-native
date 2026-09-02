# AC6 retail NTSC-U/J — real fix: `XamInputGetKeystrokeEx` returns `ERROR_EMPTY` (r181)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(165/165, up from 164/164).

## Why this check was worth running

Continuing the offline-import sweep after r180's controller-input backend,
`XamInputGetKeystrokeEx` stood out as the last remaining import in the same
`XamInput*` family (r180 covered `GetState`/`SetState`/`GetCapabilities`).

## What was found

Real contract: `DWORD XamInputGetKeystrokeEx(DWORD dwUserIndex, DWORD
dwFlags, PXINPUT_KEYSTROKE pKeystroke)` — a different shape from r180's
three imports. It polls a queue of menu-navigation "virtual key" events
(D-pad/button presses translated into keystroke-style UI-navigation
events). The real API's normal, steady-state answer when nothing new has
happened is `ERROR_EMPTY` (`0x4306`), not `ERROR_SUCCESS` with populated
output — most polls in a real title return `ERROR_EMPTY`.

This XEX's one real call site (`0x82390de0`, a thin wrapper that
normalizes an any-user-index sentinel) never inspects the return value
itself in this XEX's own code; the generic offline fallback's
`kOfflineStatus` (`0xC00000BB`) is a nonsensical NT status for this
Win32-error-shaped API regardless of that.

## Fix

Returns `0x4306` (`ERROR_EMPTY`) unconditionally — a real, valid answer in
this API's own contract requiring no output struct write (the real
contract does not populate `pKeystroke` when reporting no new event). This
project has no press/release edge-tracking event queue implemented yet
(`NativeGuestInputService`, r180, only exposes polled current-frame state,
not a discrete keystroke queue); named as a gap rather than fabricating
queued keystroke events this project cannot actually produce.

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 165/165
(164/164 before this cycle, +1 new test). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181) for further contract-shape or
   struct-fill candidates.
2. `XexCheckExecutablePrivilege` (r178) remains named, not fixed.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
