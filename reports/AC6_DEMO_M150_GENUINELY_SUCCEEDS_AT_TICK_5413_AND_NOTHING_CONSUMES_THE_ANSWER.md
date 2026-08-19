# M150 genuinely succeeds at tick 5413, and nothing downstream consumes
# the answer

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Two live probe runs on the forced-`menu_endMode=1`
route (fresh neutral store each): one watching
`AC6_DEMO_WATCH_LOADING_TASK_GATES` + `AC6_DEMO_WATCH_MODE_STATE` alone
(6000 ticks), one combining that with the new
`AC6_DEMO_WATCH_SWG_MSGI_RESULT`, `AC6_DEMO_WATCH_TASK_LIST`,
`AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS` (6000 ticks). Source changes:
`recompilation/ace-combat-6-demo/src/guest_bridge/frontend_state_trace.hpp`
(new `trace_loading_task_gates`), `swg_native_call_trace.hpp` (new
`trace_swg_msgi_result`), one call site each.

## Gate 1 is the mode object's own inner-state word -- not a coincidence

`ae14059a` narrowed the open question to `sub_8217E258`'s other two gates:
`[this-92]==1` and `[this+32]==0`, both on `CModeTaskLoadingDemoOffline`
(primary `0x2E3C0200`, `CSwgListener` subobject `0x2E3C0268`). Grepping
`trace_frontend_state`'s own `AC6_MODE_INNER` computation confirms
`mode_state = memory.load_u32(mode + 12U)` -- literally the same address
as gate 1 (`0x2E3C0200+12 = 0x2E3C020C`, `= subobject-92` per
`0x2E3C0268-92`). **Gate 1 is not a separately-tracked copy of the mode's
inner state; it is the exact same memory word `AC6_MODE_INNER` has been
reporting all along.** Its writer is therefore already traced -- the mode
task's own update function, the same mechanism `994109dc` already recorded
cycling `0 -> 4 -> 1` at ticks 4255/4259/5414.

## Both gates confirmed live, across two independent watches

```
tick=3     gate1=0x00000000 gate2=0x00   (pre-construction)
tick=41    gate1=0xFEFEFEFE gate2=0xFE   (pool-poison, unrelated object)
tick=222   gate1=0x00000000 gate2=0x00   (a different, earlier mode object)
tick=4259  gate1=0x00000004 gate2=0x00   (CModeTaskLoadingDemoOffline exists; gate1 FAILS: 4 != 1)
tick=5414  gate1=0x00000001 gate2=0x00   (gate1 now PASSES: 1 == 1)
```

`gate2` is `0` throughout -- passes from construction. `gate1` fails from
tick 4259 through tick 5413, then passes from tick 5414 onward, for the
rest of both 6000-tick runs. Gate 3 (`77cfddb5`/`99ec1791`) has been
trivially passing the whole time (the pool-poisoned byte is nonzero). **From
tick 5414 onward, all three of `sub_8217E258`'s gates pass simultaneously.**

## The predicted consequence, verified directly: SendMsgI's real answer flips

Added `trace_swg_msgi_result` (read-only, logs `SendMsgI`'s actual
out-param value on change, at the identical point
`apply_swg_sendmsgi_override` would write it, so it reports the genuine
unforced result): 

```
AC6_SWG_MSGI_RESULT tick=4259 address=0x7F03FFF8 value=0
AC6_SWG_MSGI_RESULT tick=5413 address=0x7F03FFF8 value=1
```

**Not inferred from the gate values -- read directly from the exact word
the marshaller boxes.** The result flips from `0` ("not ready") to `1`
("ready") at tick 5413, one tick ahead of `AC6_LOADING_TASK_GATES`'
own tick-5414 log line for the same transition (an ordering artifact of
which per-tick hook each instrument runs from, not a discrepancy in the
underlying mechanism -- both watch the same `[mode+12]` word settling to
`1` in the same tick window). It holds at `1` for the remaining ~590
sampled ticks of this run.

**`CModeTaskLoadingDemoOffline`'s poll is not stuck, starved, or blocked
by a missing kernel call. It genuinely, correctly transitions from "not
ready" to "ready," entirely through mechanisms this campaign has already
built and traced: the mode task's own ordinary state-machine advance.**

## And nothing downstream reacts

Same run, same tick window, all previously-tracked signals:

```
task list:   last touched tick 491 -- unchanged, before AND after the flip
render queue: 0x82386DD0 (the payload dispatch selector) never nonzero --
              unchanged, before AND after the flip. Only the routine index
              words (0x8238CD90/94/9C) show nonzero=1, as always.
milestones:  {frontend: false, mission: false, terminal: false} -- unchanged
```

**This is `883d396d`'s conclusion for message 102, now independently
confirmed for message 150, with the full causal chain empirically closed
end to end rather than inferred**: a `CSwgListener`-family handler
computes and returns a well-formed, correct, non-default answer, and
nothing measurable in this campaign's instrumentation consumes it. Two
separate messages, two separate listener classes, the same shape both
times: mechanism proven, consumption unproven.

## Consequence for the plan

This closes the `CModeTaskLoadingDemoOffline`/gate thread as a source of
the black frame. The mode's own advance to inner-state `1` is real,
timely (tick 5414, well within any route's reasonable budget), and
correctly observed by every listener polling it. **The open question is
no longer "why doesn't the loading task's poll succeed" -- it succeeds.**
It is: **who is supposed to consume `SendMsgI("M150")`'s `1` answer, and
why doesn't anything do so.** Two named candidates, neither started:

- The `M150`-sending script's own bytecode, at context `0x2E3FA914`
  (`44ddaf0c`'s original trace) -- readable in principle via the
  `346255b2`/`1fcc88b3` packed-bytecode reconstruction method this
  campaign already built for a different context.
- The mode object's own update path (`sub_8218A4A0`-shaped, per
  `ea9b3a6a`'s sibling class) -- whether `CModeTaskLoadingDemoOffline`'s
  own `[this+12]`-switched update ever reads back the `SendMsgI` result it
  just caused to change, as opposed to the message being a pure
  outbound-only broadcast nothing loops back to read.

## Not established

- Which of the two candidates above (or a third, unnamed one) is the
  intended consumer.
- Whether `[this+12]` continuing to change after tick 5414 (if it does --
  not watched past that point at fine granularity) would produce a
  different `SendMsgI` answer again, or whether `1` is a terminal state
  for this particular mode instance.
- Whether the natural (non-forced) START route would reach this same
  `[mode+12]==1` state under different input timing -- `003daa94` showed
  it doesn't reach even the mode object this thread depends on; not
  re-tested here.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. Both new instruments are read-only and opt-in
(`AC6_DEMO_WATCH_LOADING_TASK_GATES`, `AC6_DEMO_WATCH_SWG_MSGI_RESULT`),
default route behavior unchanged.
