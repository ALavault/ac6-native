# AC6 retail NTSC-U/J — `ExCreateThread` now honors `CreationFlags`; reduces but does not eliminate r112-r114's crashes (r115)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle** (not
reverted): `tools/materialize_native_import_stubs.py` gained suspended-
creation support for `ExCreateThread` plus `NtResumeThread`/
`KeResumeThread`; `tests/test_materialize_native_import_stubs.py`
gained matching tests and one existing test's capture-list assertion
was updated to match.

## Implementing r114's named next step

r114 traced two reproducible SIGSEGVs to a null guest function pointer
invoked through `PPC_CALL_INDIRECT_FUNC`'s unguarded lookup, and traced
*why* the pointer is null to `ExCreateThread`'s stub never reading
`CreationFlags` (r9, the 7th of the real Xbox 360 XDK's seven integer
arguments) -- every one of the eighteen threads this harness spawns
runs immediately, regardless of what the guest requested.

Confirmed both `NtResumeThread` and `KeResumeThread` are real imports
this XEX actually uses (`grep` on `ppc_recomp_shared.h`:
`PPC_EXTERN_FUNC(__imp__NtResumeThread)` at line 19516,
`PPC_EXTERN_FUNC(__imp__KeResumeThread)` at line 19667) before
implementing anything -- this is a real, exercised gap, not a
speculative one. `NtSuspendThread` is not imported, consistent with the
suspend-at-creation-only design below.

## Implementation

`tools/materialize_native_import_stubs.py`:

1. A new `park_until_resumed(key)` helper, distinct from the existing
   `wait_event(key)`: `wait_event` deliberately bounds its wait to 2ms
   and returns on timeout (r91's own documented contract for guest
   `Nt*Wait*` polling) -- reusing it here would let a suspended
   thread's guest code run after 2ms regardless of whether a real
   resume happened, reintroducing exactly the race this cycle is
   fixing. `park_until_resumed` uses an unbounded
   `g_event_cv.wait(lock, predicate)` instead, correctly modeling
   "blocked until resumed" with no artificial timeout.
2. `ExCreateThread` now reads `creation_flags = ctx.r9.u32`. When bit
   `0x00000004` (`CREATE_SUSPENDED`, the same value Win32 uses -- the
   real Xbox 360 XDK is documented publicly to mirror this convention;
   flagged here as external knowledge not sourced from a file in this
   repo, per r114's own caveat, and worth an independent check) is set,
   the thread's own handle is registered as an unsignaled event before
   the `std::thread` is spawned, and the spawned thread calls
   `park_until_resumed(handle)` before ever invoking the guest `shim`.
   Thread handles already share one monotonic counter
   (`g_next_handle`) with every other kernel-object handle this harness
   issues, so reusing the same `g_events` map introduces no key
   collision.
3. `NtResumeThread`/`KeResumeThread` both call `set_event(ctx.r3.u32)`
   (the thread handle) and, if a non-null out-pointer is given in
   `r4`, write the previous suspend count as `1` (was still parked) or
   `0` (already running) -- this project's harness never creates a
   thread with more than one pending suspension, so no nested-count
   semantics are modeled or asserted.

`tests/test_materialize_native_import_stubs.py`: three new tests
(`test_thread_create_honors_creation_flags_suspended_bit`,
`test_park_until_resumed_blocks_indefinitely_not_bounded` -- which
specifically asserts `wait_for` does *not* appear in
`park_until_resumed`'s body, guarding against exactly the
bounded-wait mistake described above --
`test_resume_thread_releases_a_parked_thread`); one existing test
(`test_thread_binding_dispatches_only_generated_guest_targets`) updated
for the lambda's new capture list. Full suite: 21/21 in this file, 134/134
across the retail-native pytest tree (was 131/131 before this cycle's
3 additions).

## Verified live: real reduction, not a full fix

Rebuilt via `tools/build.py --target ntsc-uj --profile native`
(regenerates `native-import-stubs.cpp` from the fixed script). Ran the
same fifteen-repetition `gdb --batch` sweep r112-r114 used:

```
attempt  1-4:   no crash
attempt  5:     CRASH -- sub_821D6C20   (r100's original site)
attempt  6-13:  no crash
attempt 14:     CRASH -- sub_821D4C20   (r112/r114's second site)
attempt 15:     no crash
```

`sub_82346428` (r111-r114's first-named site) did not crash in this
batch. `sub_821D4C20` and the original `sub_821D6C20` (r113's finding)
both still crashed, at a lower apparent rate than before (2 of 15 here,
versus 4 of 10 in r112's comparable sweep) but **not eliminated**. This
is not enough samples to claim a precise rate reduction with
confidence -- reported as directional, not quantified.

## Decision

This is real, independently-justified progress: `ExCreateThread` now
honors a real XDK parameter it previously silently discarded, backed
by verified test coverage, and the fix measurably changed at least one
crash site's behavior in this cycle's sample. It is **not** a complete
fix for r112-r114's crash family. Two explanations are open and
unestablished this cycle: (1) not every one of the eighteen threads
this game spawns is actually created with `CREATE_SUSPENDED` -- some
threads reaching a null function-pointer call may be doing so for a
different reason entirely, unrelated to suspension; (2) the resume
ordering itself may have its own race independent of suspension (e.g.
the thread meant to call `NtResumeThread` on another has its own
startup dependency this fix doesn't address). Guessing which is
refused, per this project's standard.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. `git status` (run after `ctest`, not before):
no regenerated metrics artefact came back modified-but-unstaged.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged. Python suite: 134/134 (was 131/131).

## Next

1. Instrument (temporarily, per r105's validated build-tree technique)
   which of the eighteen `ExCreateThread` calls actually set
   `CREATE_SUSPENDED` and which don't, to test explanation (1) above
   directly rather than continuing to guess from crash-site symptoms.
2. For whichever thread(s) still crash despite suspension now being
   honored, trace what real resume-ordering dependency (if any) this
   fix doesn't yet model -- e.g. does the resuming thread itself need
   to reach a specific point before it can safely call
   `NtResumeThread`, and does *that* thread have its own timing
   sensitivity.
3. Re-run a larger unattended sweep (r113's own suggestion) once a
   next hypothesis is ready to test, rather than continuing small
   interactive batches -- fifteen runs is not enough to distinguish a
   real reduction from sampling noise.
