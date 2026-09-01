# AC6 retail NTSC-U/J — r117's `case 3` target was the wrong branch; the real bailout is gated by a global mode byte, unexamined until now (r118)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed this cycle.** Two rounds of
`AC6_R118_DIAG`-gated instrumentation, across two generated files
(`ppc_recomp.22.cpp`, `ppc_recomp.23.cpp`), were added to and fully
reverted -- confirmed via `grep -c "r118"` returning 0 in both and
`ctest` 9/9 passing afterward.

## r117 assumed the wrong dispatch state -- corrected here, by name

r117 traced the post-GATE2 loop's `sub_821CC508` failure to `case 3`
of its internal `switch(r11.u64)` dispatch, citing r109's original
capture (`state(+324)=3`) as evidence for which case applies. **That
state value was never re-verified under the current, r108/r116-fixed
build** -- r117 only confirmed the *return value* (`-1`) and the
bailout branch being taken, and carried r109's older state number
forward without checking it.

This cycle re-instrumented both the loop's own state read and
`sub_821CC508`'s internal case-3 body in the same run. Result:

```
[r118] loop entry state(+324)=0
[r118] loop entry ret=-1
```

**State is `0`, not `3`, in the current build.** The state value is not
a fixed property of this code path -- it depends on everything that
ran before it, and r108's `MmQueryStatistics` fix and r116's real
critical sections both changed earlier execution enough to shift it.
r117's `case 3` analysis (the `ERROR_IO_PENDING`/997-style status
check) is a real, correctly-read piece of this function, but **it is
not the code path this run actually takes**, and citing it as "the"
crash mechanism would have been wrong. Naming this correction
explicitly, per this project's own discipline (cycles 1133/1134 in
`CLAUDE.md`, and this session's own precedent from r109/r110/r112).

## Tracing state 0 leads back to a global-flag gate r109 already named but never followed

Case `0` (`loc_821CC5BC`, `generated/ppc_recomp.22.cpp` line 35574)
does real cleanup work, then falls through
`loc_821CC5E0 -> loc_821CC5E4 -> loc_821CC5E8 -> loc_821CC5EC` --
writing the state field to `2`, then `7`, then `3` in sequence (these
are also the direct switch targets for cases 2 and 7) before landing
in the *same* `case 3` body r117 already read. Adding a print at that
body's status-check line, expecting it to fire given the fallthrough
above, **it did not fire** in either instrumented run -- direct
evidence execution never reaches that far.

Re-reading `sub_821CC508` from its own entry resolved the
contradiction. Before *any* state-based dispatch, the function has two
earlier gates, both already present in r109's own transcript but never
followed further at the time:

```cpp
// entry gate
r11 = PPC_LOAD_U32(r31 + 320) & 0x1;
if (r11 == 0) { return 1; }        // "already done" fast path
// loc_821CC534
r11 = PPC_LOAD_U8(r21 - 18120);    // r21 = a fixed global base (lis r21,-32108)
if (r11 != 1) goto loc_821CC800;   // NOT the state-switch dispatch
// loc_821CC800
if (r11 != 2) goto loc_821CCD4C;   // a THIRD, distinct dispatch region
```

The state-based `switch` r105-r109 and this cycle's own case-3 reading
covers is only reached when this global byte equals exactly `1`. With
it apparently holding some other value (consistent with never reaching
either instrumented print), execution instead falls into
`loc_821CCD4C` -- a region this investigation has not examined at all
until now.

## Decision

This closes out a real dead end from earlier in this same cycle rather
than let it stand uncorrected: the `case 3` / `ERROR_IO_PENDING`
reading is accurate as a description of that code, but not of what
this run's crash actually depends on. The genuine next target is the
global mode byte at `lis r21,-32108` / displacement `-18120` (guest
address not independently confirmed by
`FindPpcAddressMaterialization.java` this cycle -- that tool matches
`lis`+`addi`/`ori` pairs, not a `lis` base combined with a load
instruction's own embedded displacement, so it found no match; the
address is known only from reading the instruction pair directly) and
the `loc_821CCD4C` dispatch it gates into.

No native code changed. This cycle's own two dead-end instrumentation
rounds (case-3 status check, never reached) are as informative by their
*absence* of output as a positive result would have been -- they are
what definitively ruled out r117's specific mechanism rather than
leaving it as an unverified assumption for a future cycle to repeat.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`recompilation/ace-combat-6-retail/`: clean (both instrumentation
rounds, across two files, fully reverted before this report, confirmed
via `grep -c "r118"` returning 0 in each).

## Next

1. Read (or instrument) what the global byte at `lis r21,-32108` /
   `-18120` actually holds and what writes it, before assuming
   `loc_821CCD4C` is where the real `-1` originates -- confirm the
   branch is actually taken, the same discipline this cycle applied to
   correct r117.
2. Trace `loc_821CCD4C` itself once the branch is confirmed live --
   it is a third, previously unexamined region of `sub_821CC508`.
3. Any future state-value citation in this arc should be re-verified
   under the current build rather than carried forward from an earlier
   report, per this cycle's own finding that the value shifts as
   earlier fixes change execution history.
