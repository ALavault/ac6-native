# AC6 retail NTSC-U/J — eighteen worker threads spawn concurrently; the crash is an indirect call through an apparently-unready function-pointer slot (r112)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native or generated code changed this cycle.**
Investigation was entirely `gdb --batch` (passive, run-to-completion
crash capture, per r111's technique) and `objdump`/`nm` against the
already-built binary -- `git status` on
`recompilation/ace-combat-6-retail/` confirmed clean before and after
(only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change).

## Continuing r111's frontier

r111 found `ExCreateThread` spawns a real detached `std::thread`
running guest code, and caught one reproducible SIGSEGV in
`sub_82346428` (via `sub_823453E8` <- `sub_821F8008`) on that
background thread. It named tracing `sub_82346428`'s exact crashing
instruction as the next step.

## The picture is bigger than one background thread

A batch of ten `gdb --batch -ex run -ex bt` repetitions of the
identical probe command (same technique as r111, more iterations) shows:

- **Eighteen `[New Thread ...]` lines per run** -- the guest spawns
  eighteen concurrent worker threads early in this boot path, not one.
- **Two distinct crash sites**, both reached through the same common
  thread-entry trampoline (`sub_821F8008`, confirmed as the bottom
  guest frame in every capture):
  - `sub_82346428` (via `sub_823453E8`) -- 2 of 10 runs in this batch.
  - `sub_821D4C20` (directly under `sub_821F8008`, one fewer frame) --
    2 of 10 runs in this batch.
- The other 6 of 10 runs in this batch produced no crash within the
  15s bound.

This is consistent with a genuine data race across the eighteen
concurrently-launched threads rather than a single deterministic bug in
one function -- multiple independent guest worker routines, reached via
the same trampoline, each capable of crashing depending on scheduling.

## The exact crashing instruction, captured live

A further batch of repeated `gdb --batch` runs (crash rate itself
fluctuated: one batch of 8 reproduced the crash on the first attempt,
a subsequent batch of 18 straight attempts reproduced it zero times --
itself evidence the race's timing window is narrow and host-load
dependent, not a fixed per-run probability worth quoting as a
percentage) caught `sub_82346428`'s fault live:

```
0x0000555555ed66ae in __imp__sub_82346428 ()
=> call   *(%r12,%rax,2)                          <__imp__sub_82346428+414>
   mov    %r15,(%rbx)                              <...+418>
   mov    %rbx,%rdi                                <...+421>
   mov    %r14,%rsi                                <...+424>
   call   __imp__RtlEnterCriticalSection            <...+427>
   ...
rax = 0x0   rbx = 0x5555565c7e80   rcx = 0x0
rdx = 0x40948682   rsi = 0x7ffef7000000   rdi = 0x5555565c7e80
rbp = 0x0   rsp = 0x7ffeee713b40
```

The fault is an **indirect call through `[r12 + rax*2]`, with `rax =
0`** -- i.e. a call through whatever pointer sits at `[r12]` itself.
The instruction immediately following the (never-reached) call target
would enter a critical section (`__imp__RtlEnterCriticalSection`) on
the same object pair (`rbx`/`r14`) -- meaning the code was in the
middle of dispatching through what looks like a vtable-style function
pointer *before* acquiring the lock meant to protect it. That ordering
-- read-then-lock rather than lock-then-read -- is exactly the shape of
an unsynchronized read racing against another thread's write, though
this cycle did not capture `r12`'s actual value (repeated attempts to
re-catch the crash with `r12` specifically requested did not reproduce
it again within this cycle's budget -- the fluctuating reproduction
rate noted above cost the remaining attempts).

## Decision

This is a real, evidence-based narrowing of r111's frontier, not a
final diagnosis. Established this cycle: (1) the boot path launches at
least eighteen real concurrent threads through `ExCreateThread`, far
more than r111's single-background-thread framing suggested; (2) there
are at least two distinct, independently reproducible crash sites, both
under the same `sub_821F8008` trampoline; (3) the `sub_82346428` crash
is precisely an indirect call through a pointer at `[r12]`, immediately
preceding a critical-section entry on the same object -- a shape
consistent with, but not proven to be, an unsynchronized-read race.
Guessing `r12`'s value or the root cause beyond what was directly
observed is refused, per this project's standard.

## Gates

No native or generated code changed. `ctest` (native profile): 9/9
(sanity check only, since nothing was edited). `git status` on
`recompilation/ace-combat-6-retail/`: clean, unchanged from before this
cycle.

## Next

1. Re-attempt capturing `r12`'s value at the `sub_82346428` crash (this
   cycle's attempts to add it did not catch the race again within
   budget) -- a null or clearly-out-of-range value would confirm the
   unsynchronized-read-before-lock hypothesis directly; a valid-looking
   pointer would refute it and point elsewhere.
2. Separately trace `sub_821D4C20`'s crash (the second confirmed site)
   -- it is reached one frame closer to `sub_821F8008` than
   `sub_82346428`'s, so may be a simpler, more tractable case to root-
   cause first.
3. Given eighteen threads are launched concurrently this early, check
   whether this project's harness launches them with any ordering or
   pacing guarantee the real Xbox 360 kernel's thread-creation API
   would have provided (e.g. threads starting suspended until
   explicitly resumed) that `ExCreateThread`'s current stub
   (`tools/materialize_native_import_stubs.py` lines 271-301, per r111)
   does not model -- launching all eighteen as immediately-running
   detached threads may itself be the fidelity gap, independent of
   whatever specific pointer is unready in `sub_82346428`/`sub_821D4C20`.
