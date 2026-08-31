# AC6 retail NTSC-U/J — a verified NtReleaseMutant busy-spin blocked worker-thread progress; fixed (r90)

Date: 2026-08-31.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (consulted only to confirm build
provenance; no disassembly needed this cycle). XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `native/tools/materialize_native_import_stubs.py`
(new committed diagnostic + one new import handler),
`tests/test_materialize_native_import_stubs.py` (two new tests).

## Pivot

Per r89's decision, this cycle returns to NEXT.md's broader
scheduler/kernel/événements/VFS/XAM migration list rather than continuing
the now-closed `sub_821E6AC8` sub-thread. Per that same NEXT.md item's own
instruction ("par familles ABI avec une sonde bornée"), the first move was a
bounded, verifiable survey rather than a guess: which of the 229 retail
imports have no specific native handling, and — more importantly — which of
those are actually reached at runtime.

## Bounded probe: a permanent, env-gated import-call trace

Added `trace_offline_import(name)` to `materialize_native_import_stubs.py`'s
generated header, called from the generic fallback body (the one every
import without a specific `render_body` case falls through to). It mirrors
`native_guest_vd.cpp`'s existing `AC6_NATIVE_VD_TRACE` pattern exactly
(getenv-per-call, no caching, off by default): gated by
`AC6_NATIVE_IMPORT_TRACE=1`, prints `[offline-import] <name>` to stderr.
This is a permanent, cheap, reusable diagnostic (not a probe patch reverted
after use, unlike the single-thread experiment used in r83-r89) — it costs
one `getenv` call per generic-fallback invocation, identical in shape to
the already-committed VD trace.

Running the existing bounded entry probe (`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`,
default multi-threaded runtime, 20-second wall clock) with this trace
enabled found, out of 1,263,904 total offline-import hits:

- `NtReleaseMutant`: **631,941** calls
- `RtlNtStatusToDosError`: **631,902** calls
- everything else combined: **61** calls (33 `ObReferenceObjectByHandle`,
  8 `NtResumeThread`, the rest one-offs)

`NtReleaseMutant` was, and had no specific handler, unconditionally
returning `kOfflineStatus` (`0xC00000BB`, a failure code). A guest thread
retrying a failed mutant release ~632,000 times in 20 seconds, converting
the failure to a Win32 code (`RtlNtStatusToDosError`) each time, is a real,
measured busy-spin — not an inferred one.

## Fix

`NtReleaseMutant` and `NtReleaseSemaphore` (same missing-handler shape,
same XBOX 360 ABI: `r3` = handle, optional `r4` = pointer to receive the
previous count) now return success immediately, writing `0` to the
optional previous-count output. This matches the codebase's existing
single-guest-thread idiom already used for
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
("single guest thread until scheduler migration": no real contention is
modeled, so the operation always succeeds) — not a new synchronization
model, an application of the one already committed.

## Verification

Re-ran the identical bounded probe after the fix: offline-import hits
dropped from 1,263,904 to **100** in the same 20-second window (39
`RtlNtStatusToDosError`, the same handful of one-off imports as before, no
`NtReleaseMutant`/`NtReleaseSemaphore` hits at all). The spin is gone, not
reduced.

Separately, attached GDB (default multi-threaded runtime, no
single-thread gating) and inspected all threads at an interrupt point:

- **10 threads now exist** (previously the same probe, pre-fix, was never
  observed with this many live worker threads in this session's prior
  cycles — the busy-spinning thread was likely starving the others of a
  core).
- Several worker threads show **new, previously-unseen deep call stacks**
  contending on `g_event_mutex` via `NtSetEvent`/`NtClearEvent`/`wait_event`
  (threads 2, 5, 7, 8, 9, 10), and two threads (3, 6) show genuine engine
  call depth not seen in any prior cycle's traces:
  `__imp__sub_821F8008 → __imp__sub_821D4C20/821D4F20 → __imp__sub_821F4210`.
- **Thread 1 (the main thread) is unchanged**: still parked in exactly
  `sub_821E6AC8 ← sub_821E61A8 ← sub_821E64A8 ← sub_821E65B0 ← sub_8234F2C8`,
  the same chain r85-r89 investigated and closed with a bounded negative.
  This fix does not touch, contradict, or reopen that finding — it is a
  separate, previously-undiscovered blocker on other guest threads.

## What this means, and what is not established

This is real, verified progress on the broader migration NEXT.md called
for after r89: a genuine scheduler/synchronization gap that was silently
consuming most of the probe's CPU budget on a spin instead of letting
worker threads advance. It is not a claim of boot, title, menus, or
gameplay — the probe still times out (`exit=124`) and no new guest output
(DbgPrint, boot banners) was observed. What the newly-active worker threads
are actually doing (`sub_821F4210`, `sub_821D4C20`, `sub_821D4F20`,
`sub_821F8008` — none previously named in this project's reports) is not
established; that is a natural next target for a follow-up cycle's static
pass, not undertaken here.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on a pre-existing, unrelated
  N2 evidence mismatch (`reconstruction/ace-combat-6/src/retail_session.cpp`,
  already modified in the working tree before this cycle started, on the
  explicitly-abandoned N2 track per NEXT.md). Not touched this cycle.
- `ctest` (native profile): **9/9** passed.
- `pytest` (full retail suite): **128/128** passed (126 prior + 2 new:
  `test_mutant_and_semaphore_release_succeed_without_contention_model`,
  `test_generic_fallback_is_traceable_and_still_returns_offline_status`).
- `git status` after `ctest`: only the two intentionally-edited files
  changed (`tools/materialize_native_import_stubs.py`,
  `tests/test_materialize_native_import_stubs.py`); the build-tree
  regenerated `native-import-stubs.cpp` is correctly untracked/ignored.
  The pre-existing, unrelated `upstream/AC6_recomp` submodule pointer
  change was left untouched, as every prior cycle.

No scripts added or left behind this cycle (no Ghidra static pass was
needed).
