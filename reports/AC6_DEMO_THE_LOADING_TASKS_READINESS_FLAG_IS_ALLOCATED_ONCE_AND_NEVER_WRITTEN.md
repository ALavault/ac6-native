# The loading task's readiness flag is allocated once and never written

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Two live probe runs (`probe --until frontend`,
fresh neutral store each, forced `AC6_DEMO_FORCE_MENU_ENDMODE_ARG=1
AC6_DEMO_FORCE_MENU_ENDMODE_AT_TICK=4251`) using the pre-existing generic
write-range watcher (`AC6_DEMO_WATCH_ADDR_LO`/`AC6_DEMO_WATCH_ADDR_HI`,
`guest_bridge/event_post_set_trace.hpp`): one at `[0x189A22F0,0x189A2300)`
to 8000 ticks, one at `[0x827435F8,0x827435FC)` to 4300 ticks. No source
change. Also reads `ea9b3a6a` (2026-08-17, Claude Fable 5, a separate
same-day thread this session had not yet integrated) in full.

## Correction to my own assumption, verified before being relied on

`77cfddb5` read `sub_8217E258`'s third gate as a byte at
`[[0x827435F8] + 0x222BFE]` without knowing what `[0x827435F8]` held on
this specific route. Content search (`git log -S '827435F8'`) surfaced
`ea9b3a6a`, an unrelated same-day thread (`CModeTaskStartUpDemoOffline`'s
own state-machine trace) that had independently measured, on a *different*
route, `[0x827435F8]` resolving to `0x18980000` (`CTaskModeManager`, one of
the three permanent task-list entries). Rather than assume that value
carries over to the forced route, this cycle watched the write directly.
**It does**: `[0x827435F8]` is written exactly once, `tick=106,
value=0x18980000, lr=0x8219097C` (`sub_821908B8`), and never again through
tick 4300 -- a single init-time store, route-independent, exactly as
`ea9b3a6a` found on its own route. This confirms the computed flag address
(`0x18980000 + 0x222BFE = 0x189A22FE`) is sound, not a coincidence of a
different route's state.

## The flag byte itself: allocated once, never set

Watched `[0x189A22F0, 0x189A2300)` (16 bytes centered on the computed flag
address) across the full forced-route run, 8000 ticks (3749 past the
tick-4251 press, deep into the `M150` steady state `994109dc`/`77cfddb5`
already characterized). Total writes in the entire run: **four**, all at
**tick 4**, all value `0xFEFEFEFE` -- the same generic allocator
pool-poisoning fill this campaign has seen at the task-list region
(`44ddaf0c`'s probe, `994109dc`'s watch) and elsewhere. This is memory
being carved out of a pool, not content being written. **After tick 4,
nothing writes to this 16-byte window again -- not at the press, not once
in the following 3749 ticks.**

## Consequence for the plan

The chain is now closed end to end, each link independently verified:
the loading task's message-150 handler (`sub_8217E258`, `77cfddb5`) reads
a byte at `[0x18980000+0x222BFE]` as its final readiness gate; that
address is confirmed live (not just computed) to sit in memory that was
poisoned once at tick 4 and never touched again by any write this
campaign's generic write-range watcher can see. **The flag this loading
task polls every tick is never set by anything reachable on the forced
route.** Whichever mechanism is supposed to mark the corresponding
resource ready -- almost certainly the PAC/VFS load-completion signal the
plan's Phase 2 named -- either never runs at all on this route, or runs
somewhere the write-range watcher's window doesn't cover (a different
byte within the same large table, not the exact one this handler reads).

This is a clean, falsifiable, single-address finding -- the strongest and
most concrete this investigation has produced. The natural next step is
the falsifier `883d396d` already modeled for the sibling `SendMsgI`
mechanism: force the byte at `0x189A22FE` to nonzero and observe whether
the loading task's poll then reports "ready" and whatever comes after
that (a new task construction, a render-queue payload, a task-list
mutation) actually happens.

## Not established

- What is *supposed* to write this flag -- no writer was found because
  none exists in either watched route; the intended writer (a kernel
  import completion, a decode step, a different task's own update) is
  unidentified.
- Whether the large table this flag lives in (anchored at `0x18980000`,
  offset `0x222BFE` = ~2.14 MB in) is genuinely `CTaskModeManager`'s own
  extended data, or `CTaskModeManager`'s object merely anchors a much
  larger shared arena that other systems' state also lives in -- 2.14 MB
  is implausible as a single manager object's own field layout; not
  resolved this cycle.
- Whether forcing the byte to nonzero (the named next falsifier) actually
  changes anything downstream -- not attempted this cycle, per this
  session's own advisory guidance to census the writer before falsifying.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean before
this commit. No source changed -- both probe runs used the pre-existing
generic address-range write watcher, no new instrumentation.
