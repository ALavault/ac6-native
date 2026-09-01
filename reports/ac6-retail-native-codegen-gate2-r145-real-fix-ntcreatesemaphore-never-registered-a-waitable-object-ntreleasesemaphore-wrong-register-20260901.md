# AC6 retail NTSC-U/J — real native-runtime fix: `NtCreateSemaphore` never registered a waitable object (guaranteed-timeout bug); `NtReleaseSemaphore` used the wrong register as its output pointer (r145)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle, kept, not
reverted**: `tools/materialize_native_import_stubs.py`
(`NtCreateSemaphore`, `NtReleaseSemaphore`) and
`tests/test_materialize_native_import_stubs.py`.

## Reopening ground r144 called blocked

r144 concluded no actionable Gate 2 work was currently available and
stopped the standing loop. Before accepting that, this cycle re-examined
r142's own finding (`sub_82338388`'s wait times out and reads never-
written stack memory) one layer further: r142 established *that* the wait
times out, but not *why* -- whether it is a genuine "not yet signaled"
race or a structural guarantee. That distinction was worth checking
before writing off the whole area.

## What was found

`wait_event()` (`tools/materialize_native_import_stubs.py`) returns
`false` immediately, with no wait at all, for any handle absent from
`g_events`:

```cpp
bool wait_event(std::uint32_t key) {
  if (key == 0u) return false;
  ...
  auto it = g_events.find(key);
  if (it == g_events.end()) return false;   // <-- immediate, guaranteed
  ...
}
```

`NtCreateSemaphore` shared a generic stub with `NtCreateTimer`/
`NtCreateMutant` that only ever allocates a handle number
(`g_next_handle.fetch_add(1u)`) -- it never calls `create_event()`, so a
semaphore handle is *never* present in `g_events`. A live diagnostic
confirmed this directly: the exact handle `sub_82338388` waits on
(`0x12e`, `0x131` -- the same handles r142 traced) is created via
`NtCreateSemaphore`, and any `NtWaitForSingleObjectEx` on it is a
**guaranteed, deterministic `STATUS_TIMEOUT`**, independent of whatever
real `NtReleaseSemaphore` activity happens elsewhere in the guest. This
is a structural bug in this project's own HLE stub, not a race and not a
question of "not yet signaled at this exact moment."

A second, related bug was found while confirming the real
`NtCreateSemaphore`/`NtReleaseSemaphore` calling conventions from this
XEX's own call sites (`sub_821F5798` at `0x821F57xx`,
`sub_821F5830`/`0x821F5830`): `NtReleaseSemaphore`'s real, documented NT
signature is `(HANDLE, LONG ReleaseCount, PLONG PreviousCount)` --
`r4` is the `ReleaseCount` *integer*, `r5` is the actual output pointer.
The prior stub shared with `NtReleaseMutant` (whose own real signature,
`(HANDLE, PLONG PreviousCount)`, correctly puts its pointer in `r4`)
wrote through `r4` for *both* imports, meaning every `NtReleaseSemaphore`
call executed `PPC_STORE_U32(ctx.r4.u32, 0u)` -- a write to whatever small
integer `ReleaseCount` happened to be, not a real pointer. A genuine,
separate memory-corruption bug, independent of the missing registration.

## The fix

`NtCreateSemaphore` is now its own case: allocates the handle as before,
then calls `create_event(handle, /*manual_reset=*/false,
/*signaled=*/InitialCount > 0)` -- an auto-reset event matches a
semaphore's real "one permit consumed per successful wait" contract
directly, and `InitialCount` (confirmed at `r5` from the disassembled
call site) seeds the initial signaled state. `NtCreateTimer`/
`NtCreateMutant` keep the prior generic, handle-only behavior unchanged
-- nothing this project has traced shows either needs to participate in
the same wait/signal model, and extending the fix to them without
evidence would be exactly the kind of unverified guess this project's
discipline forbids.

`NtReleaseSemaphore` is now its own case: calls `set_event(ctx.r3.u32)`
to genuinely signal the semaphore, and writes the previous-count output
through the correct register, `r5`. `NtReleaseMutant` is untouched (its
own `r4`-as-pointer usage was already correct).

## Tests

`tests/test_materialize_native_import_stubs.py`: the existing
`test_mutant_and_semaphore_release_succeed_without_contention_model` was
split to check each import's own body independently (confirms
`NtReleaseMutant` still uses `r4`, `NtReleaseSemaphore` now uses `r5` and
never references `r4`, and both call the right primitives). A new
`test_create_semaphore_registers_a_waitable_event` confirms
`NtCreateSemaphore` calls `create_event(...)` with the right arguments
and that `NtCreateTimer`/`NtCreateMutant` do not. Full suite: 140/140
(was 139/139), 27/27 in this one file (was 26/26).

## Verified live: the fix is real, but insufficient to change the r131 crash's outcome

Rebuilt and reran the probe against the qualified ISO. A live diagnostic
(reverted before commit) confirmed the handles this whole investigation
has traced (`0x12e`, `0x131`) are indeed created via `NtCreateSemaphore`
and were previously unregistered; after the fix, they are correctly
registered and, per `InitialCount`, may already be signaled.

**The `sub_821F7C80` crash still reproduces, at the identical site,
via the identical call chain**, confirmed via `gdb` backtrace. This is
not a failure of this fix -- it is consistent with r142's own, separate
finding: `sub_82338388`'s ultimate "return value" comes from reading
`[r1+88]`, a *different* stack slot that nothing in the whole traced call
chain writes to, **regardless of whether the wait itself succeeds or
times out**. Fixing the wait's correctness does not populate that
unrelated stack slot. This cycle's fix and r142's finding are both
correct and do not contradict each other; they simply address two
independent problems, only one of which (this cycle's) was a genuine,
fixable native-runtime gap.

## Decision

This fix is kept and committed regardless of not changing the currently-
investigated crash's outcome, on its own merits: it corrects two real,
evidence-backed bugs in this project's own HLE layer (a structural
guaranteed-timeout for every semaphore wait in the whole game, and a
memory-corruption write through the wrong register), matches
documented NT API signatures confirmed against this XEX's own
disassembly, is fully tested, and could plausibly affect other,
unrelated wait/signal patterns elsewhere in the retail binary that this
investigation has not traced. This follows the same precedent as
r130-r131 (kept as real progress despite not eliminating the crash
they were investigating).

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 140/140 (was 139/139). `git status`
(after `ctest`, not before): only the two intended source files plus the
pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change --
no regenerated metrics artefact came back modified-but-unstaged.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged.

## Next

1. r144's determination stands for the two frontiers it named
   (`IM_LOAD_IMMEDIATE` translation, the `sub_821F7C80` crash's specific
   `[r1+88]` stack-content leaf) -- this cycle did not change either.
2. Whether this semaphore fix has any observable effect elsewhere in the
   probe run (a different wait now completing correctly, changing some
   other code path's behavior even though it doesn't reach this
   particular crash) has not been checked; a future cycle could look for
   new, previously-unreached log lines/behavior downstream of the now-
   correctly-signaled semaphores.
3. Continue treating "is this fix's target actually reachable/
   meaningful right now" as a check to run *before* declaring a whole
   area exhausted, the way this cycle did for r144's own conclusion --
   not just when starting a brand new investigation thread.
