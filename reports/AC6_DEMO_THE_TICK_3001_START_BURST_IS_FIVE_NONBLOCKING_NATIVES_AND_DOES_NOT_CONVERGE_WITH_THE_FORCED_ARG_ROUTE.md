# The tick-3001 START burst is five non-blocking natives; it does not
# converge with the `menu_endMode`-forced route

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run
(`probe --until frontend --max-ticks 8000 --input-at 3000,16,0,0,0,0,0,0,1
--input-at 3001,0,0,0,0,0,0,0,1`, neutral store, `AC6_DEMO_WATCH_MODE_STATE=1
AC6_DEMO_WATCH_SWG_NATIVE_CALL=1`, the exact press-and-release tuple
`634cff33`/`651e7878`/`eab92d66` qualified) plus static reads of the five
functions it dispatches into and one image-data table. No source change.

## Extends `eab92d66`: the stall is permanent through 5000 ticks past the press

`651e7878` and `eab92d66` established the stall through 3000 ticks past the
tick-3000 press. This run's 8000-tick budget covers 5000 ticks past it and
changes nothing: `AC6_MODE_INNER` state stays at `1` (last change tick 2452,
before the press) through tick 8000; `AC6_SWGW`'s `world`/`anim`/`player`
triplet is byte-identical (`0x2E3E3AD4`/`0x2E3DF0D4`/`0x2E3DFA94`) at every
500-tick sample from 2500 through 8000; `AC6_FRONTBUFFER` reports
`nonzero=0 of 921600` at every 1000-tick sample throughout. `outcome.kind =
"max_ticks"`, `completed_ticks = 8000` -- the run never traps, it simply
exhausts its tick budget while frozen. `presents: 7863` across 8000 ticks
confirms the vblank/present loop itself keeps running; nothing downstream of
it changes.

## The tick-3001 burst, now fully identified

`651e7878`'s call-graph diff named five targets dispatched via `0x820E9130`
at tick 3001 without reading them. This run's `AC6_DEMO_WATCH_SWG_NATIVE_CALL`
trace confirms the same five and adds the swg calling-convention detail
(`table_row`, `context`, `args`, `first_arg`, `tag`) for each:

```
tick=3001 target=0x820EA598 table_row=0x82386658 arg_count=0
tick=3001 target=0x820EA550 table_row=0x82386648 arg_count=0
tick=3001 target=0x820EA538 table_row=0x82386638 arg_count=0
tick=3001 target=0x820E9838 table_row=0x82386478 arg_count=1 first_arg=0x2E403110 tag=M102
tick=3001 target=0x820EA6C0 table_row=0x82386568 arg_count=1 first_arg=0x2E4032D0
```

All five read from the generated code (`ppc_recomp.6.cpp`). None blocks; all
return synchronously in the same tick:

- **`0x820EA538`** -- reads one fixed global field
  (`[[0x827E9CE0+10208]+120]`, computed from `lis r11,-32196; lwz
  r11,10208(r11); lwz r11,120(r11)`) and stores it to the out param. A plain
  getter.
- **`0x820EA550`** -- calls `sub_82095B80` then `sub_820E9300`, maps a
  nonzero/zero result to `16`/the raw value, stores to the out param. Two
  synchronous helper calls, no wait primitive in either.
- **`0x820EA598`** -- calls `sub_820E9290`, remaps result `6<->7` (a two-value
  enum swap, not read further -- immaterial to the stall per its shape), and
  stores to the out param.
- **`0x820E9838`** (`tag=M102` here) -- **the same function** the
  `menu_endMode`-forced falsifier (`44ddaf0c`'s predecessor work) dispatches
  repeatedly from tick ~4500 tagged `M150`. Static read (this session, prior
  cycle): iterate a fixed-stride table of registered objects, call each
  object's vtable slot `+32` (a generic per-tick/per-message broadcast), stop
  when a bounds helper (`sub_820CE010`) says so, write the accumulated
  32-bit result to the out param. A generic script-message broadcaster, not
  itself a screen or resource constructor -- confirmed shared machinery
  between the two routes, not evidence the two routes are the same.
- **`0x820EA6C0`** -- the deepest of the five. Takes a string pointer
  (`first_arg=0x2E4032D0`) and compares its first four bytes against literal
  `'N','U','D','_'`. On match, copies 14 bytes via `sub_821A4C70` (a generic
  optimized `memcpy`, confirmed by its length-dispatch shape, not a wait) and
  byte-compares the copy against a fixed image string at `0x82007CC4`, which
  reads as `"NUD_TONE_BANK_\0"` (dumped from `.build/Default.xex.base.bin`).
  On exact match it formats with `"%s"` (verbatim passthrough) using a
  format-string table at `0x82007144` that is immediately followed in the
  image by `_x`, `_y`, `_xscale`, `_yscale`, `_currentframe`, `_tot[alframes]`
  -- the classic ActionScript `MovieClip` built-in property names. On a
  partial/no match it instead formats `"NUD_TONE_BANK_%s"` (at `0x82007cb0`,
  immediately followed by the same `"NUD_TONE_BANK_"` string again) against
  the tail of the original string. Either way the formatted buffer is passed
  through `sub_820EA670` and then a virtual call (`self->vtable[+8]`, i.e.
  the object's third vtable slot) on a resolver object, and that call's
  return is stored to the out param. This is the swg engine's generic
  named-property resolver (`getProperty`/`getVariable`-shaped), not a
  resource-file loader -- reading `sub_820EA670` or the virtual target itself
  was not pursued this cycle (stopping at the function boundary per this
  read's own scope).

**None of the five natives dispatched at tick 3001 contains a wait, a
semaphore acquire, or a kernel call of any kind.** Every one is a
synchronous read-and-return that completes within the same guest tick.

## The convergence question is answered: no

`fd119ba4`/`44ddaf0c` treated "does the forced `menu_endMode=1` argument
simulate a genuine START press" as open. It does not. In this trace, from
tick 3001 through tick 8000, **zero further `AC6_SWG_NATIVE_CALL` lines
appear** -- not `menu_endMode`, not `0x820E9838` again, nothing. The forced
route's tick-4500+ steady-state (`0x820E9838` firing every tick, tag `M150`,
rotating buffer args) has no counterpart here at all. `651e7878` already
showed the film stops calling back into `0x8218AB98` (the state-2 advance)
after START at the mode/control-flow level; this trace shows the same thing
one layer down, at the script-native level: the burst fires once and the
interpreter goes fully quiet.

Consequence: `44ddaf0c`'s two fixes (`XamUserReadProfileSettings`
materialization, the `XMsgStartIORequest` validator's stack-padding-word
correction) sit on the forced-argument route only. Whether either underlying
kernel call fires on the natural START route in this window is not directly
observable from this trace (`AC6_DEMO_WATCH_IMPORTS` was not enabled here,
and no trap fired, so at most an *already-handled* import could have run
silently) -- but no evidence in this run suggests either does.

## Not established

- The exact string content at `0x2E4032D0` (the `first_arg` `0x820EA6C0`
  received) -- not captured; would need a targeted live memory watch.
- What `sub_820EA670` and the virtual call it feeds into do, or what the
  resolved property value is.
- Whether any kernel import (implemented or not) fires between tick 3001 and
  8000 on this route -- `AC6_DEMO_WATCH_IMPORTS` was not enabled.
- Whether the stall is caused by the *script's own bytecode* branching on
  one of these five natives' results (a condition this static read of the
  native wrappers cannot see, since the branching logic lives in swg
  bytecode content, not PPC code) or by something outside this burst
  entirely -- e.g. a vblank-driven counter or notification the port does not
  yet deliver, which would reconnect this thread to `5dc58584`'s
  `CX360UnitManager` priority rather than replace it.

## Consequence for the plan

This narrows, rather than closes, `651e7878`/`eab92d66`'s open question.
Every native the natural START path dispatches is now read and ruled out as
the blocking mechanism; the remaining candidates are strictly script-content
logic (unreadable the way PPC code is) or a port-side signal gap in the
`CX360UnitManager`/render-arm family `5dc58584` already named as priority.
The forced-`menu_endMode=1` route and the natural-START route remain two
independently valuable, non-converging paths -- Phase 1 progress on the
former does not substitute for solving the latter's stall.

## Process note

`git log -S '826DFC44' --oneline --all` and a `reports/` grep were run before
reading `sub_820EA6C0`, per `forward-chain-checks-need-content-search.md`:
`0x826DFC44` (the function's own gate check) is already traced as `AC6_SWGW`'s
`g44` field. In this run's trace `g44=0x2E3E43D4` (non-null) from tick 2500
onward, so the gate does not explain why this specific call's string-match
path is skipped -- it is not skipped; the gate passes and the match logic
runs, as read above.
