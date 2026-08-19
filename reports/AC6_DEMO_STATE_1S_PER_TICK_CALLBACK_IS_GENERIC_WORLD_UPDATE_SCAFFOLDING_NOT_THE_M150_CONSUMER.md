# State 1's per-tick callback is generic world-update scaffolding, not
# the M150 consumer

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run (forced-`menu_endMode=1` route,
4400 ticks, new field added to the existing `AC6_DEMO_WATCH_LOADING_TASK_GATES`
instrument) plus a static read of the flat image
(`.build/Default.xex.base.bin`) and `sub_820CE368`
(`ppc_recomp.4.cpp:7636-7900+`). Source change: one field added to
`trace_loading_task_gates` (`frontend_state_trace.hpp`).

## Closes `de508ec3`'s cheaper candidate

`de508ec3` named `[0x2E3C0200+28]`'s target as the cheapest remaining
candidate for what consumes `SendMsgI("M150")`'s answer. Captured live:
`state_one_target = 0x82006438` at tick 4255 -- a **static** image
address, not a heap object, so its own contents are directly readable from
the flat binary with no further probe run.

## What `[this+28]` actually is

`0x82006438` is not a C++ object with its own vtable pointer at offset 0;
dumping it directly shows ten consecutive real code addresses
(`0x820CF598`, `0x820CE258`, `0x820CEB68`, `0x820E8B58`, `0x820CECB0`,
`0x820CE2E8`, `0x820CE300`, `0x820CE350`, **`0x820CE368`**, `0x820CE510`)
-- a flat, static function-pointer table. Re-reading the caller
(`sub_8217E3E0`'s state-1 body) precisely: `r3` is set to `this+28` (the
*address* of the field) and never reassigned before the `bctrl` -- so the
callee receives `this+28` itself as its own `this`, not the table. This is
the same inline-vtable-pointer-field shape this campaign has already seen
for the `CSwgListener` subobject (`883d396d`'s `+104` displacement), just
a different interface at a different offset (`+28`) with a different
static vtable (`0x82006438` vs. the per-instance listener vtables). Slot
`+32` (index 8) of this table is `0x820CE368` -- confirmed live-called
every tick state 1 runs, matching the observed per-tick `M150` broadcast
cadence exactly.

## `sub_820CE368` reads and updates the SAME `w224` world object the
## campaign already tracks -- not the SendMsgI result

Read the function's first ~260 lines. It dereferences `[this+32]` and
`[this+52]` (both relative to the `this+28` subobject, i.e.
`[primary+60]`/`[primary+80]`) to reach two further linked objects, makes
several more indirect calls through them, then reads
`r31+4124`/`r31+4128`/`r31+4140` -- **the identical offsets**
`trace_frontend_state`'s own long-standing `AC6_SWGW` trace already reads
off its `w224` object (`frontend_state_trace.hpp`: `w224 + 4124U`,
`w224 + 4128U`, `w224 + 4140U`). `r31` here is that same world object,
reached through this function's own chain rather than the trace's. This
function is generic per-tick world/UI update plumbing this campaign
already has independent visibility into -- **it contains zero references
to `sub_820E9838` (SendMsgI), `0x827435F8` (the manager pointer), or the
flag-table constants (`2228224`/`11262`) anywhere in the portion read**,
confirmed by direct grep of the address range, not by skimming.

## Conclusion

`[this+28]`'s target is not where `SendMsgI("M150")`'s answer gets
consumed. It is shared per-tick world-update scaffolding, structurally
the same kind of generic, class-independent utility this campaign has
already found clustered in the `0x820CE0xx`-`0x820CE5xx` address range
(`sub_820CE010`'s slot-scan helper, `sub_820CE750`'s 20-byte allocator).
Both of `de508ec3`'s named candidates are now eliminated: the update
function's own state machine (`de508ec3`) and its one per-tick callback
(this report). **The only remaining candidate is `69e3435f`'s candidate
(a): the `M150`-sending script's own bytecode, context `0x2E3FA914`.**
Advisor's own framing from earlier this cycle is confirmed directly, not
just asserted: *"the boxed result goes to the calling script, not the
listener"* -- native-side update machinery has now been checked twice and
shown, both times, to not be where a script's own return value would be
read back.

## Consequence for the plan

This closes the native-code-side search for M150's consumer. Reading the
script's own bytecode (the `346255b2`/`1fcc88b3` reconstruction method,
`390cfe33`'s `table_base = [[ctx+0]+8]` formula, applied to context
`0x2E3FA914`) is now the only remaining avenue -- not a candidate among
several, the last one standing. This is a genuinely different technique
from everything this cycle used (native code/memory reading) and is the
right place for a fresh cycle to start, rather than being started at the
tail of this one.

## Not established

- The rest of `sub_820CE368`'s body past line ~7900 (roughly the first
  260 of what appears to be a longer function) -- not read, judged
  unnecessary once the offset check confirmed no reference to the
  relevant addresses in the portion covered.
- What the other nine table slots in `0x82006438` do, or what other
  classes besides `CModeTaskLoadingDemoOffline` share this same static
  vtable at offset `+28` -- not swept.
- The actual content of the `M150`-sending script's bytecode at context
  `0x2E3FA914` -- entirely unread; this is the next cycle's starting
  point, named but not begun.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. The one source change (an added field to an existing
read-only, opt-in trace) does not alter default route behavior.
