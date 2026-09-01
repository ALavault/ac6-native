# AC6 retail NTSC-U/J — real native-runtime fix: `KeSetAffinityThread` returned a negative `NTSTATUS`-shaped value where the real contract expects a small, positive affinity mask (r149)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle, kept, not
reverted**: `tools/materialize_native_import_stubs.py`
(`KeSetAffinityThread`) and `tests/test_materialize_native_import_stubs.py`.

## Following r148's own next step

r148 named reading `KeSetAffinityThread`'s call sites -- newly reached
only after that cycle's `ObReferenceObjectByHandle` fix -- before
deciding whether it needed real handling.

## What was found

Its one call site (`sub_821F3F...` region) confirms the real, reduced
Xbox 360 kernel signature: `(Handle, DWORD Affinity, DWORD* PreviousAffinity)`.
The caller does two things with the result that the generic offline
fallback breaks:

1. It checks the return value (`r3`) with `blt` as if it were an
   `NTSTATUS`. The real Windows/Xbox kernel contract for this call is
   different: `r3` carries the *previous affinity mask itself*, not a
   status code -- confirmed directly by this same caller, which computes
   `31 - countLeadingZeros(mask)` against the *separate* `*PreviousAffinity`
   output to find "which core index was previously set". A real mask on
   a 6-hardware-thread Xenon title is always small and non-negative, so
   this `blt` guard is effectively dead code on real hardware; the
   previous, generic `kOfflineStatus` fallback (a negative value as a
   signed mask) tripped it on every call.
2. It reads `*PreviousAffinity` (`[caller_frame+84]`) for that same bit-
   scan. The generic fallback never wrote it, so even past the `blt`
   guard the bit-scan would have run over uninitialized stack memory --
   the same class of bug r148 fixed for `ObReferenceObjectByHandle`'s
   `KeResumeThread` call.

## The fix

`KeSetAffinityThread` now returns `1u` in `r3` (a real mask, not a
status code) and writes `1u` through `*PreviousAffinity` when non-null --
both consistent with "core 0", the lowest, always-in-range hardware
thread on this title's topology. This project models no real per-core
thread affinity (a single host-scheduled execution model, matching every
other thread-management stub already in this file), so there is no
genuine previous mask to report; a small, in-range, internally-consistent
value keeps the caller's own bit-scan meaningful instead of reading
uninitialized memory or tripping a guard that real hardware would never
trip.

## Tests

`tests/test_materialize_native_import_stubs.py`: new
`test_ke_set_affinity_thread_returns_a_real_mask_not_a_status` confirms
the generated body writes `1u` through `r5` and returns `1u` in `r3`,
with no `kOfflineStatus` reference. Full suite: 142/142 (was 141/141),
29/29 in this one file (was 28/28).

## Verified live: real behavioral change confirmed, same crash site persists

Rebuilt and reran the probe with `AC6_NATIVE_IMPORT_TRACE=1`.
`KeSetAffinityThread` no longer appears as an unhandled import.
`RtlNtStatusToDosError`'s own "unmapped status" count -- a proxy for how
often this run hits an error path it cannot convert to a real Win32
code -- dropped from 19 to 2 in the same run, consistent with this
call chain now completing successfully rather than repeatedly erroring
out. The `sub_821F7C80` crash still reproduces at the identical site via
the identical call chain (`gdb` backtrace confirmed) -- expected, since
its own causal chain (r130-r142) is unrelated to thread-affinity
handling.

## Decision

Kept and committed on its own merits, the same precedent as r145/r148:
a real, evidence-backed contract mismatch is fixed (a raw mask
misinterpreted as a status code, plus an unwritten output parameter),
directly justified by this XEX's own single call site, fully tested, and
measurably changes observable guest behavior (fewer unmapped-status
error paths hit) even though it does not resolve the specific crash
this investigation continues to track separately.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 142/142 (was 141/141). `git status`
(after `ctest`, not before): only the two intended source files plus the
pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged.

## Next

1. `ObDereferenceObject` (35 calls) and `KeSetBasePriorityThread`
   (17 calls) remain the two most frequent unhandled imports in this
   run. Neither's return value is checked at any sampled call site
   (r148's own finding), so they are lower priority than r145/r148/r149's
   fixes, but worth a quick re-check now that more code runs past them.
2. Continue checking "does this fix change observable guest behavior
   anywhere" after any real native-runtime fix, independent of whether
   it resolves the crash currently under active investigation.
3. r144's determination about the `sub_821F7C80` crash chain's specific
   `[r1+88]` stack-content leaf and `IM_LOAD_IMMEDIATE` translation both
   still stand -- this cycle did not change either.
