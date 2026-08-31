# AC6 retail NTSC-U/J — wait_event now genuinely blocks; the aggregate futex-volume question stays open (r91)

Date: 2026-08-31.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `tools/materialize_native_import_stubs.py`
(`wait_event`/`set_event`/`create_event` reworked to use a real
`std::condition_variable`), `tests/test_materialize_native_import_stubs.py`
(one new test).

## Continuation of r90's frontier

r90 identified the newly-active worker threads (`sub_821F4210`,
`sub_821D4C20`/`821D4F20`, `sub_821F8008`) as an open item. A static
disassembly pass this cycle found `sub_821F7C80` (called from
`sub_821F8008`) walks a linked list at a fixed static address invoking each
node's function pointer via `bctrl` — a plausible, ordinary
callback/notification-list dispatch, not an obviously broken loop. Rather
than build on that inference, this cycle measured instead: `strace -f -c`
on the same bounded probe (`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`, default
multi-threaded runtime, 15s) showed **1,547,456 `futex` calls**
(619,105 returning an error), all attributable to `g_event_mutex` — the
single mutex shared by `create_event`/`set_event`/`clear_event`/`wait_event`
(confirmed by GDB thread backtraces: every contended thread's top frames
were `lll_mutex_lock/unlock` on that exact symbol). This is the same shape
of finding as r90 (a measured, not inferred, high syscall-rate) applied to
the broader event-wait family the original session plan (before this
investigation branched into the Vd-specific thread) already flagged:
`wait_event()` returns immediately rather than blocking, so any caller
retrying it as fast as the host can re-issue it will busy-spin.

## Fix

`wait_event()` in the generated stub file's anonymous namespace now blocks
on a `std::condition_variable` (`g_event_cv`) for up to 2ms when the event
is not yet signaled, instead of returning `false` immediately.
`set_event()`/`create_event(..., signaled=true)` now call
`g_event_cv.notify_all()` after mutating state. The single-shot, non-blocking
-forever contract every caller depends on is unchanged: `NtWaitForSingleObjectEx`
et al. still resolve to `STATUS_TIMEOUT` (`0x102`) if the wait wasn't
satisfied within the bounded window — only the *mechanism* changed, from an
instant poll to a real (short, bounded) block.

## Verification: the specific mechanism is proven; the aggregate claim is not

**Proven, live**: attaching GDB post-fix and inspecting all threads found
one genuinely inside `pthread_cond_wait` on `g_event_cv` (not spinning),
confirming the blocking path executes as written — this is a direct
observation, not an inference from the source.

**Proven, live, via hit-counted breakpoints** (`ignore N 999999999` on each
guest-facing event import, 12s window, same bounded probe): the event
family is called at a real, substantial rate —
`NtSetEvent`: 13,873; `NtClearEvent`: 11,619; `wait_event`: 34,509;
`NtCreateEvent`: 24; `KeSetEvent`/`KeResetEvent`/`NtPulseEvent`: 0 — roughly
5,000 combined calls/second even under GDB's own per-hit breakpoint
overhead (which itself measurably throttles execution, since resuming from
tens of thousands of breakpoint stops per second is far from free).

**Not established**: whether this fix reduced the `strace`-measured
aggregate futex volume. Re-running the identical `strace -f -c` probe after
the fix gave **1,501,150 futex calls** (254,477 errors) — statistically
indistinguishable from the pre-fix 1,547,456 (619,105 errors) in raw count,
though the error count dropped by 59%. Comparing the GDB breakpoint-derived
call rate (~5,000/s) against the strace-derived futex rate (~100,000/s)
directly is not valid evidence either way: the two measurements were taken
under different, non-comparable instrumentation overhead (GDB's breakpoint
stop/resume cost per hit is far larger than strace's per-syscall cost, so
the program plausibly ran at very different effective speeds under each
tool). Asserting either "the fix eliminated the spin" or "the fix had no
effect" from this data would be exactly the kind of plausible-but-uncontrolled
claim the project's evidence discipline refuses. Left open.

## What this cycle actually established

- `wait_event()`'s blocking behavior is a genuine, verified architectural
  correction (matches the original session plan's Step 2 "kernel-wait
  case" recommendation, never previously implemented) — kept regardless of
  the open aggregate question, because it is strictly more correct than an
  unconditional instant return and is independently verified working.
- The event-wait family (`NtSetEvent`/`NtClearEvent`/`wait_event`) is
  genuinely exercised at a high, sustained rate across many worker
  threads — this is real traffic, not a measurement artifact.
- What is NOT established: whether that traffic is legitimate per-frame
  engine signaling (unthrottled because nothing in this offline probe
  paces it to a target frame rate — there is no vsync/timer gate yet) or a
  guest-code pattern that is genuinely spinning without a real signal ever
  arriving. Answering that needs a static trace of the actual callers of
  `NtSetEvent`/`NtClearEvent` (which guest functions, at what point in
  their control flow) — not undertaken this cycle, and the natural next
  step.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90
  (`reconstruction/ace-combat-6/src/retail_session.cpp`, abandoned track,
  not touched this cycle).
- `ctest` (native profile): **9/9** passed.
- `pytest` (full retail suite): **129/129** passed (128 prior + 1 new:
  `test_wait_event_blocks_briefly_instead_of_busy_spinning`).
- `git status` after `ctest`: only the two intentionally-edited files
  changed; the pre-existing, unrelated `upstream/AC6_recomp` submodule
  pointer change was left untouched.

No scripts left behind (`DumpR91WorkerFuncs.java`, `DumpR91Helpers.java`
were used read-only against `ghidra-projects/ac6-us` and deleted; confirmed
by `git status --porcelain=v1 -- scripts/` showing empty).
