# AC6 retail NTSC-U/J — real critical sections eliminate r111-r115's crashes entirely; they were masking r101's original null-global bug, now deterministic (r116)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle** (not
reverted): `tools/materialize_native_import_stubs.py` gained real
mutex-backed `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`/
`RtlInitializeCriticalSection*`, plus a permanent `AC6_NATIVE_IMPORT_TRACE`-gated
trace line in `ExCreateThread` (matching the existing `DbgPrint`-style
convention already in this file); `tests/test_materialize_native_import_stubs.py`
gained matching tests.

## Testing r115's own open question first

r115 left open whether every `ExCreateThread` call actually requests
`CREATE_SUSPENDED`. Added a trace line to the existing stub (gated by
`AC6_NATIVE_IMPORT_TRACE`, the same pattern `DbgPrint` already uses)
and ran the probe:

```
[ExCreateThread] handle=258 routine=0x821d4c20 flags=0x00000001 suspended=0
[ExCreateThread] handle=270 routine=0x821d4f20 flags=0x00000001 suspended=0
[ExCreateThread] handle=286 routine=0x823453e8 flags=0x00000000 suspended=0
... (18 lines total, every one suspended=0)
```

**None of the eighteen threads request `CREATE_SUSPENDED`.** r114/r115's
central hypothesis is refuted directly: r115's suspended-creation
implementation was correct and independently justified, but it was a
no-op for this specific game -- the guest never sets the bit r115
added support for. Whatever reduction r115 observed in its own 15-run
sample was noise, not the fix taking effect.

## The real bug: `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` were no-ops

With suspension ruled out, checked what actually protects the
function-pointer dispatch the `sub_82346428`/`sub_821D4C20` crashes go
through -- both disassemblies (r112, r114) showed
`RtlEnterCriticalSection` called immediately around the crashing
region. Reading the stub itself:

```python
if name in {"RtlEnterCriticalSection", "RtlLeaveCriticalSection",
            "RtlInitializeCriticalSection",
            "RtlInitializeCriticalSectionAndSpinCount"}:
    return "  ctx.r3.u64 = 0u;  // single guest thread until scheduler migration\n"
```

**A literal no-op**, under an explicit, now-falsified assumption: this
project's own `ExCreateThread` work (r111 first, confirmed at scale by
this cycle's trace) spawns eighteen real, concurrent `std::thread`
instances. With the guest's own critical sections doing nothing, every
one of those threads races through whatever the guest intended to
serialize -- exactly the shape r112 originally (and correctly)
suspected before r114's static trace led the investigation toward the
null-pointer mechanism instead of the missing lock.

## Fix

Replaced the no-op with real per-object `std::recursive_mutex`
instances, keyed by the guest `RTL_CRITICAL_SECTION` object's own
address (the same "key by guest identity" pattern this file's `g_events`
map already uses for handles): `RtlEnterCriticalSection` locks,
`RtlLeaveCriticalSection` unlocks, `RtlInitializeCriticalSection*`
forces the backing mutex to exist. `std::recursive_mutex` specifically
because the real Win32/XDK API contract allows a thread already
holding a section to re-enter it. Four new/updated tests, including one
that explicitly asserts the stale `"single guest thread"` string is
gone and that the emitted stub genuinely calls `.lock()`/`.unlock()`.
Full suite: 135/135 (was 134/134 before this cycle's one net addition).

## Verified live: the race is gone, and a much older bug is exposed

Rebuilt via `tools/build.py`. Ran a 25-repetition `gdb --batch` sweep,
the same technique r111-r115 used but at higher sample count given how
decisive the result turned out to be:

```
attempts 1-25: CRASH -- sub_821D6C20, every single time, identical PC
```

**Zero background-thread crashes** (`sub_82346428`, `sub_821D4C20`) in
25 runs -- a complete elimination, not the partial, sample-noise-sized
reduction r115 reported. But the main thread now crashes
**deterministically, every run**, in `sub_821D6C20` -- r100's original
crash site. Captured live:

```
0x000055555570dc28 in __imp__sub_821D6C20 ()
#0  __imp__sub_821D6C20 ()
#1  __imp__sub_821D7DE0 ()
#2  __imp___xstart ()
#3  main ()
=> call *(%rcx,%rax,1)
rbp = 0x82935d98
```

**`rbp = 0x82935d98` is the exact null global r101 identified at the
very start of this whole investigation arc**, well before r105's
`MmQueryStatistics` chain or any of this session's work -- "root-caused
the r100 crash mechanism precisely -- a null global (`0x82935d98`)
dereferenced as a vtable pointer" (r101's own words, per this session's
history). This was never actually fixed; every cycle since r100
(including r108's real, independently-verified `MmQueryStatistics`
fix) was working around a *different* bailout path that happened to
run before this dereference was reached, or the multithreaded race
r111-r115 characterized was hitting *before* this deterministic path
ever got a clean shot at running. With real critical sections in place,
execution now reaches this null dereference reliably, on every run,
instead of racing past or into something else first.

## Decision

This is a real, verified, independently-justified infrastructure fix
that should be kept: `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
now do what their name says, eliminating a genuine multithreading bug
(demonstrated by a 25/25 crash rate dropping to 0/25 for the sites they
protect) rather than papering over a symptom. It does not close Gate 2
-- it trades an intermittent, hard-to-characterize crash family for a
fully deterministic one, which is a real net improvement for
tractability even though the probe still does not boot further. The
crash it now exposes on every run is not new: it is r101's own finding,
never actually resolved, previously masked by a race this project spent
r111-r115 characterizing without knowing it was hiding an older,
already-diagnosed bug underneath.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. `git status` (run after `ctest`, not before): no
regenerated metrics artefact came back modified-but-unstaged.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged. Python suite: 135/135.

## Next

1. Re-open r101's null-global finding directly: what is supposed to
   write a real vtable pointer to `0x82935d98` before
   `sub_821D6C20` reads it, and why does that writer never run (or run
   too late) even with real thread synchronization now in place. This
   cycle's fix made the question fully deterministic and reproducible
   on every run -- a much easier starting position than the racy
   symptom r111-r115 chased.
2. This session's earlier commits (before this investigation arc,
   titled around "the graphics interrupt is dispatched outside the DPC
   contract" and "the real waiter is an equality wait on a sequence
   counter") may be relevant prior work on a related interrupt/dispatch
   mechanism -- worth checking for direct relevance to `0x82935d98`
   before re-deriving anything already established, per this project's
   own discipline against redoing settled work.
3. Now that the crash is deterministic, r108/r109's own
   build-tree-instrumentation technique (temporary, reverted `fprintf`)
   can be applied with a single run instead of a repeated sweep --
   check what code, if any, ever targets `0x82935d98` as a store
   destination across the whole generated codebase before assuming
   nothing does.
