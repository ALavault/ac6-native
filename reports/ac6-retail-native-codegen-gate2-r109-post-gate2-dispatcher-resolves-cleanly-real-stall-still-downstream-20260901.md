# AC6 retail NTSC-U/J — the post-GATE2 dispatcher resolves in one call, not a spin loop; the real stall is further downstream (r109)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** Two rounds of
temporary diagnostics were added to and fully reverted from the
generated (gitignored, untracked) `ppc_recomp.23.cpp` -- confirmed via
`grep -c "r109"` returning 0 and `ctest` 9/9 passing afterward.

## Continuing r108's frontier

r108 closed the entire r100-r107 crash chain: GATE2 (`sub_821F4078`,
wrapping `MmAllocatePhysicalMemoryEx`) now succeeds, and the bounded
entry probe advances past it with no crash, hitting the 30s timeout
bound instead. r108 named the next step as the standing plan's own Step
1 -- statically verify what the guest waits on now -- but flagged that
the call graph past GATE2 had not been re-traced since r100's crash
made the old `sub_821E6AC8` thread moot.

## A plausible-looking loop, checked directly rather than assumed

Reading `Function_821D5F48` (`generated/ppc_recomp.23.cpp`) forward from
GATE2's success path (past `loc_821D6144`) turns up, at line 19143, a
loop shape matching this project's own documented pattern for a
direct-memory-poll stall (the demo track's superseded `reports/cycle-1829..1833`
finding, cited in the standing plan):

```
loc_821D6358:
	// mr r3,r29
	ctx.r3.u64 = r29.u64;
	// bl 0x821cc508
	sub_821CC508(ctx, base);
	// cmpwi cr6,r3,0
	cr6.compare<int32_t>(ctx.r3.s32, 0, xer);
	// beq cr6,0x821d6358
	if (cr6.eq) goto loc_821D6358;
	// blt cr6,0x821d6138
	if (cr6.lt) goto loc_821D6138;
```

`sub_821CC508` (`generated/ppc_recomp.22.cpp` line 35428) itself
contains an internal state-dispatch loop reading a field at `r31+324`
(the same struct, `r29+324`) via a 10-entry function-pointer jump table
built from a base at `-2112028672 + -14968`, plausible-looking as *the*
poll this project has been hunting since r51. A `grep` across its
~1600-line body found no `__imp__*Wait*`/`__imp__Nt*`/`__imp__Ke*`
calls, consistent with a pure memory-poll shape rather than a kernel
wait.

**This was checked live rather than asserted from the shape alone**,
per this project's own instrument-before-trusting discipline. Two
rounds of temporary `AC6_R109_DIAG`-gated `fprintf` instrumentation
(first at the `loc_821D6358` loop head printing the state field and
`sub_821CC508`'s return value; second, after the first round showed
nothing printed within 15s, fourteen numbered checkpoints after every
`bl` between GATE2's success branch and `loc_821D6358`) showed:

```
[r109] checkpoint 1
[r109] checkpoint 2
...
[r109] checkpoint 14
[r109] call=0 state(+324)=3 ret=-1
```

All fourteen intervening calls return promptly (no hang before the
loop). The loop itself executes **exactly once**: `sub_821CC508`
returns `-1` (negative) on its first call, which takes the `blt
-> loc_821D6138` branch immediately -- the same shared bailout GATE2
itself used to take before r108's fix, this time returning
`Function_821D5F48`'s `r3 = 0` cleanly, not looping and not crashing.

## Decision

**The hypothesis this loop is the stall is refuted by direct
measurement**, in the same spirit as this project's standard for
killing a plausible-but-uncontrolled rule (`CLAUDE.md` cites cycles
1111/1113). The loop's shape alone looked exactly like the kind of
poll this project has been looking for since the demo track's r1829-1833
work -- and it would have been easy to report it as the answer without
running it. It is not: `sub_821CC508` fails fast (state `3`, return
`-1`) and control returns from `Function_821D5F48` to its caller
(`sub_821D7DE0`, per r102's mapping) within microseconds of GATE2
succeeding. The actual 30-second stall this project has now confirmed
twice (r108, r109) happens **after** `Function_821D5F48` returns, in
code this cycle did not reach with instrumentation.

No native code changed -- this cycle only ruled out one candidate
location for the stall, precisely, rather than leaving it as an
untested assumption for a future cycle to repeat.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (both diagnostic rounds
fully reverted before this report, confirmed via `grep -c "r109"`
returning 0).

## Next

1. `sub_821D7DE0` (r102's mapping: `Function_821D5F48` is "exactly the
   first thing `sub_821D7DE0` calls") is the immediate next candidate --
   trace what it does with `Function_821D5F48`'s `r3 = 0` return and
   what it calls next. This project's own r102 already found the caller
   "checks-but-doesn't-abort on failure," so the call graph should
   continue forward from there, not restart from `_xstart`.
2. If that forward trace is long, prefer the r105/r108/r109-validated
   technique (build-tree `fprintf` instrumentation, gated, reverted
   before commit) over GDB, which r104 formally retired for this probe.
   A checkpoint-numbering sweep (as used in this cycle's second round)
   is a fast, low-risk way to bound where in a large function a stall
   actually begins before hand-tracing every instruction.
