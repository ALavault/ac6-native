# AC6 retail NTSC-U/J — the bounded entry probe is run-to-run nondeterministic even without GDB; r108/r109 need a caveat (r110)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** Three rounds of
temporary `AC6_R110_DIAG`-gated diagnostics were added to and fully
reverted from the generated (gitignored, untracked) `ppc_recomp.23.cpp`
-- confirmed via `grep -c "r110"` returning 0 and `ctest` 9/9 passing
afterward.

## Continuing r109's frontier, and finding a scoping error in it

r109 concluded `Function_821D5F48` (`sub_821D5F48`) "returns cleanly...
within microseconds" after its post-GATE2 dispatch loop resolved with a
negative return, based on the absence of further instrumentation output
after the loop. **That inference was not actually verified** -- r109's
instrumentation covered only the loop head itself, not what came after
it, so "no more prints" was consistent with either the bailout branch
genuinely returning, or execution falling through into the ~1140 lines
of `sub_821D5F48` that follow the loop (the function spans
`generated/ppc_recomp.23.cpp` lines 18586-20291 -- `loc_821D6138` and
`loc_821D6358`, all of r105-r109's GATE2/loop work, are internal labels
inside this single very large function, not separate ones).

This cycle traced forward from `sub_821D7DE0` (r108's own named next
step) and added markers that distinguish the two paths directly, rather
than inferring from absence. First attempt: markers at `sub_821D7DE0`'s
entry and immediately after its call to `sub_821D5F48` -- one run
printed only the entry marker and hung. Second attempt: added markers
at `loc_821D6138`'s `return` and at the loop's fallthrough path. **This
run printed the full sequence**, confirming the bailout branch is
genuinely taken and `sub_821D5F48` does return -- r109's conclusion
about the loop itself holds:

```
[r110] sub_821D7DE0 entry
[r110] loc_821D6358 call=0 ret=-1
[r110] loc_821D6138 bailout return, sub_821D5F48 EXITS here
[r110] after sub_821D5F48
[r110] pre-warm loop iter r31=2
```

## But the same binary, same inputs, same instrumentation gives different results run to run

A longer (60s) rerun of the exact same instrumented binary **segfaulted**
after printing only the entry marker -- a third, different outcome. Five
repeated runs of the identical binary with identical environment and a
fixed 20s bound were then captured explicitly:

| run | outcome | last marker printed |
|-----|---------|----------------------|
| 1 | timeout | `sub_821D7DE0 entry` only |
| 2 | timeout | `pre-warm loop iter r31=2` |
| 3 | timeout | `sub_821D7DE0 entry` only |
| 4 | timeout | `pre-warm loop iter r31=2` |
| 5 | timeout | `sub_821D7DE0 entry` only |

**3 of 5 runs stall inside `sub_821D5F48`, before ever reaching the
post-GATE2 loop this project has spent r105-r109 characterizing; 2 of 5
reach past it to the pre-warm loop.** No GDB was involved in any of
these five runs -- this is real, native, run-to-run nondeterminism in
the probe itself, distinct from the GDB-specific unreliability r104
already retired that technique for.

## Decision

This corrects r109 by name on a real scoping error (it inferred a
return from absent output rather than observing the return directly),
though not on its substantive conclusion -- the loop genuinely does
resolve without spinning, confirmed directly this cycle. More
importantly, **it adds a caveat r108 and r109 did not carry**: both
reports describe a single run's outcome (GATE2 succeeds with no crash;
the post-GATE2 loop resolves quickly) as though it were the probe's
behavior. This cycle shows a majority of runs (3/5) don't even reach
that point -- something earlier in `sub_821D5F48` (before line 19145,
GATE1 through GATE5's own checks, per r105's naming) is itself
nondeterministic, stalling on some runs and not others with identical
inputs.

The mechanism is not established this cycle -- candidates include
another still-genuinely-uninitialized stack read whose stale-byte
content depends on host process layout (ASLR, allocator behavior) that
differs run to run despite `GuestAddressSpace`'s zero-fill-on-first-touch
guarantee, or a timing-sensitive check reading a wall-clock/counter
value. Guessing which is refused, per this project's own standard
(cycles 1111/1113 cited in `CLAUDE.md`) for a plausible-but-uncontrolled
claim.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (all three diagnostic
rounds fully reverted before this report, confirmed via `grep -c
"r110"` returning 0).

## Next

1. **Any future cycle's probe conclusion needs multiple runs, not one**,
   before being reported as the probe's behavior -- this cycle's own
   five-run sweep is the template. A single "no crash" or "resolves
   cleanly" run is not enough evidence on this specific probe, now that
   nondeterminism is confirmed independent of GDB.
2. Characterize the nondeterminism itself before chasing the pre-warm
   loop / `sub_821D6C20` frontier further: instrument the earlier
   portion of `sub_821D5F48` (GATE1-GATE5, lines ~18586-19145) across
   several runs to find which specific check diverges between a
   stalling run and a progressing one -- the same checkpoint-numbering
   technique this cycle and r109 both used, applied to the region
   before the loop instead of after it.
3. r108's fix (`MmQueryStatistics`) remains correct and load-bearing --
   nothing in this cycle contradicts it. This is specifically about
   what happens after GATE2 succeeds, not about GATE2 itself.
