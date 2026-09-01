# AC6 retail NTSC-U/J — r101's null global `0x82935d98` is written by `sub_821D5F48` itself, near its very end; a premature bailout is why it never runs (r117)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle, used
read-only for `FindPpcAddressMaterialization.java`), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** One round of
`AC6_R117_DIAG`-gated instrumentation was added to and fully reverted
from the generated (gitignored, untracked) `ppc_recomp.23.cpp` --
confirmed via `grep -c "r117"` returning 0 and `ctest` 9/9 passing
afterward.

## Continuing r116's frontier

r116's fix (real `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`)
made the crash r101 originally found fully deterministic: every run
now crashes in `sub_821D6C20`, dereferencing a still-null global at
`0x82935d98`. r116 named reopening that address directly as the next
step, and noted the crash is now reproducible in a single run rather
than needing a repeated sweep.

## Where the global is written

`scripts/FindPpcAddressMaterialization.java` (read-only, no Ghidra
project modification) against `0x82935d98` found **exactly one**
`lis`/`addi` materialization in the whole XEX:

```
821d6be0 lis r11,-0x7d6d ; 821d6be4 addi r11,r11,0x5d98 => 0x82935d98
```

The corresponding generated code
(`generated/ppc_recomp.23.cpp`, `loc_821D6BE0`, line ~20262) is:

```cpp
loc_821D6BE0:
	r11.s64 = -2104295424;      // lis r11,-32109
	r11.s64 = r11.s64 + 23960;  // addi r11,r11,23960  => 0x82935d98
	PPC_STORE_U32(r11.u32 + 0, ctx.r3.u32);  // stw r3,0(r11)
```

**This is the only write to `0x82935d98` anywhere in the title.** It
sits inside `__imp__sub_821D5F48` -- the same ~1700-line function
(`generated/ppc_recomp.23.cpp` lines 18584-20287) r105-r109 already
spent an entire cycle arc characterizing (GATE1 through GATE5, the
post-GATE2 dispatch loop at `loc_821D6358`), near its very end, about
1100 lines *after* that loop.

## Confirmed live, in one deterministic run: the write is never reached

Since r116 made the crash 100% reproducible, a single instrumented run
(not a repeated sweep) sufficed. Added markers at the post-GATE2 loop's
result, the shared bailout label `loc_821D6138`, and the write site
itself:

```
[r117] post-GATE2 dispatch ret=-1
[r117] BAILOUT sub_821D5F48 exits early via loc_821D6138 -- 0x82935d98 NEVER written
```

(process then segfaults in `sub_821D6C20`, exactly as every prior
cycle's crash captures showed). **The full causal chain is now closed
end to end**:

1. `sub_821D5F48`'s post-GATE2 dispatcher (`sub_821CC508`, r109's own
   naming) returns `-1` for guest state `3` -- exactly the value r109
   captured in isolation, now reconfirmed live with the write-site
   instrumentation attached in the same run.
2. That `-1` return takes the `blt -> loc_821D6138` branch (r109's
   "resolves without spinning" finding still holds -- the loop itself
   does not hang) -- the same shared bailout GATE1/GATE2 used to use
   before r108's fix.
3. `loc_821D6138` executes a genuine C++ `return;`, exiting
   `sub_821D5F48` **about 1100 lines before** the `0x82935d98` write,
   which is never reached.
4. `sub_821D7DE0` (the caller) does not abort on this failure --
   r102's original finding, still accurate -- and proceeds to call
   `sub_821D6C20` regardless.
5. `sub_821D6C20` reads `0x82935d98`, finds it still zero, and
   dereferences it as a vtable pointer -- the crash r100 first found
   and every cycle since has, one way or another, reached.

## A first look at what state 3 is actually trying to do

Read (statically, not yet instrumented) `sub_821CC508`'s `case 3`
target (`loc_821CC5EC`, `generated/ppc_recomp.22.cpp` line 35600
onward): it is a substantial routine computing sizes from fields at
`r31+328`/`+340`/`+22884`/`+22888`, then calling `sub_821F4170` (result
stored at `r31+22880`) followed by `sub_821F3BF0` -- the shape of a
buffer/resource-pool allocation and initialization, not a simple state
check. Where within this routine the `-1` actually originates was not
traced this cycle; the routine continues well past what was read.

## Decision

This closes, precisely and with a single deterministic instrumented
run, the question r105 opened and r106-r116 progressively narrowed:
`0x82935d98` is not written because the one function that would write
it exits early via a bailout branch already characterized in detail by
r109. This is not a new mechanism -- it is the same `sub_821CC508`
state-3 failure r109 found, now understood to be the actual root cause
of r100's original crash, not a separate concern. Continuing to chase
`sub_821D6C20`'s own crash site (as r101, r113, and this session's
early captures did) was always downstream of this; the real next
question is why `sub_821CC508`'s state-3 handler fails.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (instrumentation fully
reverted before this report, confirmed via `grep -c "r117"` returning
0).

## Next

1. Trace `sub_821CC508`'s `case 3` routine (`loc_821CC5EC` onward,
   `generated/ppc_recomp.22.cpp` line 35600) to its actual return
   point(s) and find which specific check produces `-1`. Given its
   apparent shape (buffer/pool allocation via `sub_821F4170` +
   `sub_821F3BF0`), a failed allocation is a plausible candidate --
   check the same class of harness gap r108 found (an unimplemented or
   under-modeled import feeding it a value that causes a real failure
   path to trigger) before assuming anything about the guest's own
   logic being at fault.
2. Once the `-1` cause is found and fixed (if it is a harness gap) or
   otherwise resolved, re-verify live that `0x82935d98` actually gets
   written and that `sub_821D6C20` no longer crashes, using the same
   single-run instrumentation technique this cycle validated as
   sufficient now that the crash is deterministic.
