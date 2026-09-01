# AC6 retail NTSC-U/J — `RtlNtStatusToDosError` implemented and verified live; confirmed necessary but not sufficient alone, exactly as r125 predicted (r126)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle** (not
reverted): `tools/materialize_native_import_stubs.py` gained a
dedicated `RtlNtStatusToDosError` case; `tests/test_materialize_native_import_stubs.py`
gained a matching test.

## Implementing the first half of r125's two-part fix

r125 closed the full causal chain from r100's crash to two specific
unimplemented imports, and named implementing `RtlNtStatusToDosError`
as the safer, lower-risk half to do first -- a pure, stateless,
well-documented NTSTATUS -> Win32 conversion with no state and no risk
of masking a harness gap the way fabricating file data would.

## Implementation

Added a dedicated case to `render_body()`: `STATUS_SUCCESS (0) ->
ERROR_SUCCESS (0)`, `STATUS_PENDING (0x103) -> ERROR_IO_PENDING (997)`
-- the two values this project has directly traced as relevant
(r109 through r125) -- and, for anything else, `ERROR_MR_MID_NOT_FOUND`
(317), which is real Windows NT's own documented default return for a
status with no explicit table entry, not a guessed value. The
`default` branch is traced through the existing
`AC6_NATIVE_IMPORT_TRACE` convention, printing the unmapped status so
a future cycle can see exactly which other NTSTATUS values this title
actually converts before deciding whether more entries are needed.

`tests/test_materialize_native_import_stubs.py` gained
`test_nt_status_to_dos_error_maps_pending_to_io_pending`, asserting
both explicit mappings, the `317` default, and that the stub's own
body never falls back to `kOfflineStatus` (i.e. that this import is
genuinely handled, not silently routed to the generic fallback).
Full suite: 23/23 in this file (was 22/22), 136/136 across the
retail-native pytest tree (was 134/134).

## Verified live: works exactly as designed, and exactly as predicted -- necessary, not sufficient

Rebuilt via `tools/build.py --target ntsc-uj --profile native`. Ran the
probe with `AC6_NATIVE_IMPORT_TRACE=1`:

```
     43 [RtlNtStatusToDosError] unmapped status=0xc00000bb
```

All 43 calls (matching r121's own count exactly) convert
`0xc00000bb` (`kOfflineStatus`, still coming from `NtReadFile`'s own
still-unimplemented stub) and correctly fall through to the documented
`317` default rather than silently passing the sentinel through
unchanged -- the fix works precisely as designed.

**As r125 explicitly predicted, this alone does not stop the crash**
-- `NtReadFile` still needs its own fix before this conversion ever
sees a real `STATUS_PENDING` to turn into `997`. Confirmed with a
direct `gdb --batch` capture: the crash is unchanged, same
deterministic site as every cycle since r116:

```
0x000055555570dc28 in __imp__sub_821D6C20 ()
#0  __imp__sub_821D6C20 ()
#1  __imp__sub_821D7DE0 ()
#2  __imp___xstart ()
#3  main ()
```

## Decision

This is real, independently-justified, low-risk infrastructure --
correct regardless of whether `NtReadFile`'s own fix ever lands -- and
its behavior was verified live to match r125's mechanism exactly, not
merely compiled and assumed correct. Keeping it does not risk masking
anything: unmapped statuses fall back to a real, documented default
and are traced, rather than silently returning a project-internal
sentinel as if it were a real Win32 error code.

`NtReadFile` itself remains unimplemented, deliberately: r125 already
named the real design constraint (a stub that claims `STATUS_PENDING`
forever would trade this deterministic crash for an infinite
busy-loop, since the retry counter only decrements on a recognized
failure, not a pending status) and r122/r123 already flagged the
remaining uncertainty about what file (if any) this specific call
chain is meant to read. Implementing it without resolving that is
exactly the kind of rushed step this project's own precedent (r115's
self-flagged caveat) warns against.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. `git status` (run after `ctest`, not before):
no regenerated metrics artefact came back modified-but-unstaged.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged. Python suite: 136/136 (was 134/134).

## Next

1. Trace where `sub_821F4E70`'s file handle (its own `r3` argument,
   `r30` in the disassembly) originates -- which `NtCreateFile` call
   feeds this specific retry-loop chain, and whether it is the same
   `Function_82390F48`/hard-drive-partition cluster r122/r123 traced or
   a genuinely separate, still-unfound call site. This determines
   whether a real file read is even meaningful here or whether the
   correct real-hardware behavior is a clean failure this harness
   should also produce (in which case `0x82935d98` never getting
   written might be the *correct*, expected real-hardware outcome, and
   the actual gap would be elsewhere -- e.g. `sub_821D6C20` failing to
   null-check before it dereferences, which real hardware's own
   equivalent code presumably does not need to because the write always
   happens there).
2. Design `NtReadFile`'s fix only once (1) is resolved -- returning
   `STATUS_PENDING` on first issue (matching r124's confirmed caller
   expectation) plus a genuine second-poll completion, backed by real
   data if a real file is identified, or a principled decision to model
   this specific operation as failing cleanly if it corresponds to
   hardware this disc-based title's harness has no reason to emulate.
