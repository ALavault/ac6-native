# AC6 retail NTSC-U/J — cosmetic contract-shape fixes: `RtlTryEnterCriticalSection`/`KeEnterCriticalRegion`/`KeLeaveCriticalRegion` (r165)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with matching tests. Verified via a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 9/9), the full retail-native pytest
suite (148/148, up from 145/145), and a live import trace confirming all
three imports no longer reach the generic offline-import fallback.

## What was done

r164's scan explicitly named these three as the remaining candidates in
the same NTSTATUS-shaped-sentinel bug family fixed in r162/r163
(`ObDereferenceObject`/`KeSetBasePriorityThread`/
`KeQueryBasePriorityThread`), and explicitly found neither currently
changes observed control flow — this cycle implements the fix anyway, on
the same defensive-correctness principle r162 already established: a
status-shaped sentinel should never leak out of an import whose real
contract has no status to report, regardless of whether today's traced
callers happen not to depend on the distinction.

- **`KeEnterCriticalRegion`/`KeLeaveCriticalRegion`** (real contract:
  `VOID`) now return `0u` instead of `kOfflineStatus` — a value shape a
  `VOID`-returning import should never have produced, even though r164
  confirmed no traced caller reads it.
- **`RtlTryEnterCriticalSection`** (real contract: `BOOLEAN`, nonzero =
  lock acquired) now returns a canonical `1u` instead of `kOfflineStatus`
  — both values are nonzero and evaluate identically under every traced
  caller's zero-vs-nonzero test, but `1` is the correct, in-range shape
  for a "lock always available" stub (this project models no real
  per-thread contention), where `kOfflineStatus` is an NTSTATUS-shaped
  accident that happened to be truthy.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 148/148
(145/145 before this cycle, +3 new tests: a parametrized
`test_critical_region_enter_leave_are_void_not_a_status` covering both
`KeEnterCriticalRegion`/`KeLeaveCriticalRegion`, plus
`test_rtl_try_enter_critical_section_returns_a_real_boolean_not_a_status`).
Live import trace confirms none of the three reach
`trace_offline_import()` any more. `gdb` backtrace confirms the tracked
`sub_821F7C80` crash still reproduces identically, exactly as expected for
a change r164 already established has no observed control-flow effect.
`git status` clean apart from the intended change set.

## Decision

This closes out the full NTSTATUS-shaped-sentinel-vs-real-contract
correctness sweep this session's r148 originated: every offline-import
stub this campaign's own live trace has ever surfaced at any call-volume
tier has now been checked against its real Xbox 360 kernel contract shape,
and every genuine mismatch found has been fixed (r148, r162, r163, r165).
No further candidates remain in this specific bug family.

## Next

Both of Gate 2's named frontiers are unchanged: DATA.TBL chain fully
traced and closed (r144/r153/r161); `IM_LOAD_IMMEDIATE`→SPIR-V
policy-blocked pending Xenos fetch-signature qualification. With the
contract-shape sweep complete, the remaining named options are the same
as r164 left them: a genuinely new investment (the pinned Wine/Xenia route
for NTSC-U/J, not yet attempted), or a different frontier entirely.
