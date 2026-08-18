# The forced-`menu_endMode=1` route's steady state is permanent, and
# touches neither the task list nor the render queue

## Correction, folded in rather than filed separately

`09f752c7`'s own "Not established" section named `3c7e7291`'s gate field
(`[sub112+8]`, the word gating `GetCurrentMission`'s `16` fallback) as "the
highest-value open thread." That framing was stale at the moment it was
written: `ee81086d` (2026-08-19, earlier in this same history) had already
corrected the branch read that produced it -- `16` is the value
`GetCurrentMission` returns when its gate *succeeds*, never observed live in
this campaign, not a forced fallback -- and `6fc7b184` had already force-
tested the `16` result directly with no effect on completion. Do not chase
that thread; this line retracts it in place rather than as a standalone
report, per this cycle's own advisory review.

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run (`probe --until frontend
--max-ticks 16000 --backend headless`, fresh neutral store,
`AC6_DEMO_FORCE_MENU_ENDMODE_ARG=1 AC6_DEMO_FORCE_MENU_ENDMODE_AT_TICK=4251
AC6_DEMO_WATCH_TASK_LIST=1 AC6_DEMO_WATCH_MODE_STATE=1
AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS=1 AC6_DEMO_WATCH_SWG_NATIVE_CALL=1`, all
five pre-existing instruments, no source change). Answers the two questions
`44ddaf0c` explicitly left open.

## What this closes: the tick-4500+ loop is a genuine, permanent steady state

`44ddaf0c` ran this exact falsifier recipe to its then-6000-tick budget and
flagged, unresolved: *"Whether `frontend=true` is reachable by continuing to
run past 6000 ticks from this exact state, or whether the tick-4500+ loop is
a genuine steady-state."* This run extends the budget to 16000 ticks --
10,000+ ticks, nearly three minutes of simulated runtime, past the forced
press. Result:

```
outcome  = {kind: "max_ticks", completed_ticks: 16000}
milestones = {presents: 15863, frontend: false, mission: false, terminal: false}
```

`AC6_DEMO_WATCH_SWG_NATIVE_CALL` confirms the `M150`-tagged call to
`0x820E9838` is still firing every single tick at the very end of the run
(tick 15999, same `table_row=0x82386478`, `context=0x2E3FA914`, rotating
`args` among the same three addresses `44ddaf0c` already observed). This is
not progress toward a milestone -- it is the same loop, unbroken, for the
entire extended window. `frontend=true` is not reachable by running longer
from this state.

`AC6_DEMO_WATCH_MODE_STATE` adds one genuinely new fact: the mode object
*does* switch once, for real, after the forced argument -- `AC6_MODE_SWITCH
tick=4255 mode=0x2E3C0200 vtable=0x82011154 previous=0x2E3C0100` -- and its
inner state cycles `0->4` (tick 4259) before settling at `state=1` at tick
5414. **No further `AC6_MODE_SWITCH` or `AC6_MODE_INNER` line appears for
the remaining 10,586 ticks.** So the forced route is not inert at the mode
level -- it genuinely constructs and switches to a new mode object, distinct
from `29da1b05`'s injected-dispatch traps, which never even got this far --
it just stops one level short of whatever would come next, and stays there.

## The render-queue payload selector: still always zero, now over 15,748 samples

`AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS` covers `[0x82386D90, 0x8238CDD0)`, which
includes both the slot array `09f752c7` traced (base `0x82386D90`, stride 96)
and the producer/consumer index pair (`0x8238CD90`/`0x8238CD94`) one level
higher in the same struct. Filtering to slot0's own dispatch-selector field,
`0x82386DD0` (`09f752c7`'s `slot+64`): **15,748 stores across the run, every
one `nonzero=0`**, all from the producer's own write site
(`sub_820FF710`, `lr=0x820FF734`, `generated_line=46508`) -- zero hits from
any other writer, and zero `nonzero=1` occurrences anywhere in the entire
16000-tick trace. `09f752c7`'s "the queue is starved" finding was established
on a narrower window; this run confirms it holds unchanged 10,000+ ticks into
the one route this campaign has found that gets furthest past the title
screen. The only addresses that ever go nonzero in this watch are the
bookkeeping words `0x8238CD90`/`0x8238CD94` (the producer/consumer indices,
expected to advance) and `0x8238CD9C` (`kRenderQueueBase+0x60DC`, one word
past the consumer index -- not identified this cycle, not the payload
selector). The scheduler's own aggregate counters in this run's report
(`producer_changes: 31495, consumer_changes: 0`) reproduce `75c5d1ac`'s
already-explained quantum-sampling artifact exactly -- expected, not new.

## New finding: the task-list dispatcher goes silent at tick 491 and never
## wakes again

`AC6_DEMO_WATCH_TASK_LIST` covers the 64-byte task-dispatcher region
`cycle-1777` named (`0x18970400`-`0x1897043C`, guest address space, 16
words). Across the full 16000-tick run, **every single access to this region
-- load or store, any of the 16 addresses -- occurs at tick 491 or earlier.
The last line in the entire trace touching this region is a `kind=load32` at
tick 491.** Not one access occurs afterward: not at the forced press (tick
4251), not during the mode switch (tick 4255-5414), not anywhere in the
10,586-tick steady state that follows. The forced-argument route reaches a
real, distinct mode object and a stable inner state, but never once touches
the structure that would hold a constructed menu or mission task.

## Consequence for the plan

Both of `44ddaf0c`'s open questions are now closed, and both close the same
way: **the forced route is real, makes genuine progress (a mode switch,
not a no-op), and then plateaus permanently one level short of anything
this campaign has been able to observe as "constructed."** It does not
starve at the render queue by getting stuck before reaching it -- the
render queue was never going to be reached this way, because the task list
that would need a new owner for the render queue to have anything to draw
is never touched again either. Both symptoms are downstream of the same
plateau, not two separate failures.

The one live, still-firing mechanism this forced route continuously drives,
with no visible effect, is `0x820E9838`'s `M150` broadcast -- already
statically read in `44ddaf0c`: iterate a fixed-stride table of registered
objects, call each one's vtable slot `+32`, stop via a bounds helper
(`sub_820CE010`), accumulate a result. What is actually registered in that
table on this route, and whether it is empty, populated with inert
animation-clip objects, or (the interesting case) populated with something
that *would* construct a menu owner if its own precondition were met, is
the concrete next question -- the one mechanism proven still warm and
reachable at the exact point everything else goes cold.

## Not established

- What is registered in `0x820E9838`'s target table at `table_row
  =0x82386478` on this route -- count, identities, and whether any entry's
  vtable slot `+32` handler does anything beyond animation-buffer bookkeeping.
- What `0x8238CD9C` (`kRenderQueueBase+0x60DC`) is.
- What the `"all started guest threads blocked before frontend milestone"`
  frontier diagnostic (`lr=0x822e559c`, `address=0x822f8848`, all 23 threads
  reported blocked, 0 runnable, at the exact tick-16000 snapshot) means --
  whether this is the routine between-tick parked state every run exhibits at
  a sampling instant, or a genuinely new fact about this route. Not
  distinguished this cycle; flagged so it is not silently read either way.
- Whether the natural (non-forced) START route ever reaches this same mode
  switch (`0x2E3C0100`->`0x2E3C0200`) under any input timing -- `003daa94`
  showed the natural tick-3001 burst does not, but a different natural
  press timing was not re-tested here.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean before
this commit, no source changed (report-only, all instrumentation used
pre-existed this cycle).
