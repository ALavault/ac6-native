# AC6 retail NTSC-U/J — the r112 fault pointer is confirmed unmapped garbage, and the original main-thread `sub_821D6C20` crash still happens intermittently after r108 (r113)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native or generated code changed this cycle** --
investigation was entirely `gdb --batch` (passive crash capture, same
technique as r111/r112) against the already-built binary. `git status`
on `recompilation/ace-combat-6-retail/` confirmed clean before and
after (only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change).

## Continuing r112's frontier: `r12`'s value at the `sub_82346428` fault

A batch of `gdb --batch` repetitions reproduced the `sub_82346428`
crash r112 characterized and, this time, captured `r12` directly:

```
0x0000555555ed66ae in __imp__sub_82346428 ()
#0  __imp__sub_82346428 ()
#1  __imp__sub_823453E8 ()
#2  __imp__sub_821F8008 ()
#3  ?? () from libstdc++.so.6
#4  start_thread
#5  __GI___clone3
r12 = 0x7ffe75980000
(gdb) x/4gx $r12
0x7ffe75980000: Cannot access memory at address 0x7ffe75980000
```

**`r12` is confirmed unmapped -- not a null pointer, and not a valid
pointer into any live region of this process.** This is a real data
point, not a guess: r112's "read-before-lock" shape is consistent with
either a race on a legitimate pointer, or a genuinely uninitialized
register/memory value that happens to look pointer-shaped. `0x7ffe7598...`
sits in the same general host-address neighborhood as other thread
stacks seen in this session's captures (e.g. `0x7ffef7000000`-class
values recur across unrelated registers in multiple crash dumps),
which is at least consistent with -- though does not prove -- a stale
copy of some other thread's stack-relative value read into `r12`
before it was ever meaningfully assigned by this thread's own code
path.

## A second, more consequential finding: the original `sub_821D6C20` crash still happens, on the main thread, after r108

While repeating the capture, one run produced a **different** crash
entirely -- on the main thread, not a background one:

```
Thread 1 "ac6recomp" received signal SIGSEGV, Segmentation fault.
0x000055555570dbc8 in __imp__sub_821D6C20 ()
#0  __imp__sub_821D6C20 ()
#1  __imp__sub_821D7DE0 ()
#2  __imp___xstart ()
#3  main ()
r12 = 0x7fffffffdb01   (readable: 0x3800007fffffffdb 0x0300000000000000 ...)
r13 = 0x555555765250
r14 = 0x7ffef7000000
r15 = 0x7fff79aa0000
rax = 0x7fff79aa0000
rbx = 0x7fffffffd780
```

`sub_821D6C20` is the exact symbol r100 first found crashing, and which
r108's fix and r108/r109's own probe runs showed no longer crashing (a
timeout with no SIGSEGV was reported in every run checked at the time).
**This single capture shows that conclusion does not hold universally**:
the same crash site can still fault, on the main thread, intermittently
-- not on every run, but real. This did not reproduce again in a
follow-up batch of fifteen further attempts within this cycle's budget,
consistent with the same kind of narrow, load-sensitive timing window
r112 already documented for the background-thread crashes.

This is unlike r112's `sub_82346428` case in one respect: here `r12`
*is* readable, holding plausible-looking (if unexplained) byte values
-- so whatever is wrong is not simply "the pointer is unmapped." The
exact faulting instruction and what it dereferences was not captured
this cycle (the `x/10i $pc` request in this run's command was not
retained in the log actually reviewed -- only the register dump was).

## Decision

Two real, separately-evidenced findings, neither fully closed:

1. **r112's crash mechanism is refined, not resolved**: `r12` at the
   `sub_82346428` fault is confirmed unmapped garbage, ruling out "it's
   a valid pointer to the wrong object" and supporting (without yet
   proving) a genuinely-uninitialized-value origin, in the same general
   class as r108's `MmQueryStatistics` finding -- but for a different,
   unidentified field.
2. **r108's fix does not fully close the original crash chain.**
   r108/r109's "no crash observed" reports each described a small
   number of runs, and this cycle's single main-thread `sub_821D6C20`
   capture shows the crash this project has been chasing since r100 can
   still occur. This does not contradict r108's own specific claim (the
   `MmQueryStatistics` fix genuinely made GATE2's allocation succeed,
   verified directly and reproducibly) -- but it does mean "the crash
   chain is closed" was never established as a universal claim, and
   should not be treated as one going forward.

Neither crash's root byte-level cause is established this cycle.
Guessing is refused, per this project's standard.

## Gates

No native or generated code changed. `ctest` (native profile) not
re-run this cycle (no edits to verify); prior cycle's 9/9 stands.
`git status` on `recompilation/ace-combat-6-retail/`: clean, unchanged
from before this cycle.

## Next

1. Recapture the `sub_821D6C20` main-thread crash with full
   disassembly (`x/10i $pc`) to identify the exact faulting
   instruction and operand, the same way r112 did for `sub_82346428` --
   this cycle only got as far as the register dump.
2. Given the timing-sensitivity both crash sites share, consider
   running a longer unattended sweep (tens of repetitions in the
   background, tolerant of the ~10-20% observed hit rate) rather than
   small interactive batches, to gather enough samples for both sites
   without spending an entire cycle's budget on retries.
3. r112's own next steps (tracing `sub_821D4C20`, checking whether
   `ExCreateThread` should launch its eighteen threads suspended rather
   than immediately running) remain open and unaffected by this cycle's
   findings.
