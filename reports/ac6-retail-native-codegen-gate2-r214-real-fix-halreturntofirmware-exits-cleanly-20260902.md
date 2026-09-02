# AC6 retail NTSC-U/J — real fix: `HalReturnToFirmware` exits cleanly (r214)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(200/200, up from 199/199).

## What was found

`VOID HalReturnToFirmware(HAL_RETURN_TYPE Routine)` is a documented NT
kernel API that reboots or halts the console and never returns. This
XEX's single real call site (`0x821f7e6c`, `r3=1`, conditionally gated
behind an earlier check) is weaker evidence than r193/r209/r213's
never-returns findings: the compiler **did** emit a normal epilogue
after this call (`addi r1,r1,0x60; ...; blr`), unlike those three cases
where no epilogue existed at all. That does not disprove the documented
"never returns" contract — a compiler unaware a callee never returns
still generates a defensive epilogue regardless — it just means this
call site alone cannot independently confirm it the way the other three
did.

## Fix

Calls `std::exit(0)` — the same class of fix as r209's
`XamLoaderTerminateTitle`, since `HalReturnToFirmware` is a
whole-console-level exit (not per-thread, so r213's
`GuestThreadTerminated` unwind does not apply here), matching the real,
documented semantics rather than falling through to whatever the
compiler's own (never-reached-on-real-hardware) epilogue does.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
200/200 (199/199 before this cycle, +1 new test,
`test_hal_return_to_firmware_exits_cleanly`). `git status` unchanged
apart from the intended change set and the same pre-existing, unrelated
dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r213's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r214).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
