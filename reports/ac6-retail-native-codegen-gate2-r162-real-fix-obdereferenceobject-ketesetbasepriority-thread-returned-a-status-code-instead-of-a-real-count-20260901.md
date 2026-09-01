# AC6 retail NTSC-U/J — real fix: `ObDereferenceObject`/`KeSetBasePriorityThread` returned an NTSTATUS-shaped sentinel instead of the real plain-`LONG` contract (r162)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used for this fix (real disassembly is the evidence). Real code
change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`
(the hand-maintained generator source), with matching test additions.
Verified via a clean `tools/build.py --target ntsc-uj --profile native`
(`ctest` 9/9), the full retail-native pytest suite (144/144, up from
142/142), and a live import trace confirming both imports no longer
appear as unhandled "offline-import" hits.

## Why this check was worth running

With the DATA.TBL live-comparison arc closed (r161), this cycle returned
to r149's own explicitly-named, previously-deprioritized next candidates:
"`ObDereferenceObject`/`KeSetBasePriorityThread` (lower priority, returns
never checked)". A live import trace confirmed these are this build's two
most-frequently-hit unimplemented imports (`ObDereferenceObject` 35 times,
`KeSetBasePriorityThread` 17 times in one bounded probe run) — high
call-volume, previously unaddressed.

## What was found: the same contract-shape bug r148 already fixed once

Both imports' real Xbox 360/Windows NT kernel contracts return a plain,
small `LONG` — `ObDereferenceObject(PVOID Object)` returns the object's
new reference count; `KeSetBasePriorityThread(PKTHREAD, LONG Increment)`
returns the *previous* increment. Neither is NTSTATUS-shaped. The generic
offline-import fallback both previously fell through to returns
`kOfflineStatus` (`0xC00000BB`), an NTSTATUS-shaped negative sentinel —
exactly the same category of bug r148 fixed for `KeSetAffinityThread`.

Static verification against all of this XEX's own real call sites (18 for
`ObDereferenceObject`, 3 for `KeSetBasePriorityThread`, found via
`Ac6Xrefs.java` against the real import thunk addresses `0x823D00DC` /
`0x823D00EC`, then read via `Ac6XenonDisasm`): **every single traced call
site discards the return value immediately** — either overwritten by an
unrelated value before the function returns, or the call is the last thing
done before an unconditional `li r3,<constant>`. Confirmed at
`sub_821F3DA0`, `sub_821F3EA0`, `sub_821F3F30` (all three of
`ObDereferenceObject`'s and `KeSetBasePriorityThread`'s shared call sites),
plus a spot-check of two further `ObDereferenceObject`-only sites. This
matches — and confirms, rather than merely repeats — the deprioritization
note already in this file's own comments ("never dereferenced by guest
code itself").

## Decision: fix the contract shape anyway

Unlike `KeSetAffinityThread` (r148), where the wrong shape was actually
*read* by a real caller's `< 0` guard, no currently-traced caller of either
import here depends on the return value at all — this fix has no observed
behavioral effect on the current probe. It is made anyway, on the same
principle r148 already established: an NTSTATUS-shaped sentinel is the
wrong shape for a plain-count-returning kernel call regardless of whether
today's traced callers happen to discard it, and a future code path (a
caller not yet reached by this project's bounded probe, or a title update)
could inherit a status-shaped value from an import that should never
produce one. Both stubs now return `0u` — a safe, in-range, non-negative
value consistent with each real contract's shape, with no output-parameter
writes needed (neither real signature has one).

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 144/144
(142/142 before this cycle, +2 new tests:
`test_ob_dereference_object_returns_a_plain_count_not_a_status`,
`test_ke_set_base_priority_thread_returns_a_plain_increment_not_a_status`).
Live import trace confirms both imports no longer reach
`trace_offline_import()`. `gdb` backtrace confirms the tracked
`sub_821F7C80` crash still reproduces identically (unrelated chain,
unaffected by this fix, exactly as expected for a low-call-volume-impact
correctness fix like this one). `git status` clean apart from the intended
change set.

## Next

1. `KeQueryBasePriorityThread` remains an unaddressed offline-import stub
   in the same family (queries rather than sets base priority) — not yet
   checked whether it shares the same NTSTATUS-shape bug.
2. Both of Gate 2's named frontiers are unchanged: DATA.TBL chain fully
   traced and closed (r144/r153/r161); `IM_LOAD_IMMEDIATE`→SPIR-V
   policy-blocked pending Xenos fetch-signature qualification.
