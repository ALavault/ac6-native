# AC6 retail NTSC-U/J — real fix: `ObCreateSymbolicLink`/`ObDeleteSymbolicLink` always succeed (r196)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(183/183, up from 182/182).

## What was found

`NTSTATUS ObCreateSymbolicLink(POBJECT_STRING SymbolicLinkName,
POBJECT_STRING DeviceName)` and `NTSTATUS
ObDeleteSymbolicLink(POBJECT_STRING SymbolicLinkName)` are standard,
documented NT kernel APIs used to register/remove a device drive-letter
mapping. `ObCreateSymbolicLink` has 2 real call sites, `ObDeleteSymbolicLink`
has 4.

One traced directly: real call site `0x821ea034` (inside
`Function_821E9F50`, real range `[0x821e9f50, 0x821ea2f3]`) sits inside
what is unmistakably a device-mount retry loop — on failure (`cmpwi
r3,0x0; bge ...` skips the error path only on non-negative return) it logs
a diagnostic and jumps back to `0x821e9f74` to try the next mount
candidate. The generic offline no-op's `kOfflineStatus`
(`0xC00000BB`, negative) took the error branch **unconditionally**, so
this loop could never observe a successful mount through this call — a
real boot-sequence blocker (this is squarely in the "reach Mission01
gameplay" territory Gate 2 is still working toward), not a cosmetic
status-shape difference.

## Fix

Both now unconditionally return `STATUS_SUCCESS` (`0`). This project's own
path resolution (`guest_path_to_relative`, used throughout
`NtCreateFile`/`NtOpenFile`-family imports) never consults a registered
symlink table — paths are resolved by stripping any drive-style prefix
directly, independent of whatever `ObCreateSymbolicLink` would have
registered on real hardware. So there is no real lookup this fix needs to
back with data; it is the same "native side already owns this
subsystem's lifecycle" precedent already used for `VdRetrainEDRAM` and
similar Vd lifecycle no-ops.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
183/183 (182/182 before this cycle, +1 new test,
`test_ob_symbolic_link_registration_always_succeeds`). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r195's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r196).
2. If Mission01 native gameplay observation is ever attempted again and
   still stalls before boot completes, the other 3 real
   `ObDeleteSymbolicLink` call sites and the second `ObCreateSymbolicLink`
   call site (both not traced individually this cycle) are worth
   re-checking first, since the fix here is now unconditional success for
   both imports regardless of caller.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
