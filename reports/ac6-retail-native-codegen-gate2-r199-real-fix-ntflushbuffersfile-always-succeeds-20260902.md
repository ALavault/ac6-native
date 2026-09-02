# AC6 retail NTSC-U/J — real fix: `NtFlushBuffersFile` always succeeds (r199)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(186/186, up from 185/185).

## What was found

`NTSTATUS NtFlushBuffersFile(HANDLE FileHandle, PIO_STATUS_BLOCK
IoStatusBlock)` is a standard, documented NT kernel API. This XEX has 2
real call sites (`0x82392848`, inside `Function_82392838`; `0x8239130c`,
inside `Function_82391248`). Both pass a handle and an `IoStatusBlock`
output pointer that neither call site reads back afterward: the first
checks only success/failure via `blt` (negative = error); the second
discards the return value entirely, falling straight into the next
instruction with no check at all.

This project's guest media is read-only (r189/r190, and r197's
`NtQueryInformationFile` finding) — there is never a pending write for a
real flush to reconcile, so unconditional success is the honest, traced
contract for this build, not a guess.

## Fix

Returns `STATUS_SUCCESS` (`0`) unconditionally. `IoStatusBlock` is left
unwritten since neither real call site reads it back — same "don't
assert a field nothing consumes" discipline as r189's untouched
timestamp fields.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
186/186 (185/185 before this cycle, +1 new test,
`test_nt_flush_buffers_file_always_succeeds`). `git status` unchanged
apart from the intended change set and the same pre-existing, unrelated
dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r198's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r199).
2. If this project ever adds real guest write support, revisit whether
   `NtFlushBuffersFile` still needs to be a true no-op or should
   reconcile a real pending-write queue.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
