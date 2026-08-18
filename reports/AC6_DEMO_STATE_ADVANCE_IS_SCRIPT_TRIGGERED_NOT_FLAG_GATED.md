# The full chain: an ActionScript-triggered listener pump advances state, and title's movie never reaches the cue that fires it

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence:
`codegen/generated/ppc_recomp.{4,6,14,15}.cpp` (control-flow evidence
only). Reachability control: the same valid 12000-tick, START-at-3000
atlas (`a1d0106e`/`39dc4038`) used throughout this session, including its
`indirect_edges` block. Live evidence: a new richer write-watch instrument
(`trace_addr_range_write`, `guest_bridge.cpp`/`event_post_set_trace.hpp`,
added this cycle) confirming the static reading against a real run. No
oracle.

## What this closes

`fbd898c1` narrowed the search to `[CSwgManager+24]`'s `vtable+116` call
and two other unconditional calls in `sub_820CE368`, having ruled out the
flag-gated block entirely (startup's own flag never pulses either, yet its
state still advances). Reading those calls in full (all three: `CNuTimer`
getter — pure, no writes; the `helper.vtable+116` stopwatch accumulator —
writes only `[helper+8]`/`[helper+12]`; `player.vtable+60` — a 512-element
per-tick animation-timer loop) **exhausts every write reachable from state
1's own handler, two levels deep, and none touch task state.** This forced
the conclusion that the write comes from entirely outside this call graph
— confirmed below.

## Building the instrument that found it

`guest_memory.cpp`'s `watch_addr_range_host_write` (added `bdb437e6`) only
has `__builtin_return_address`, which resolves every guest store to the
same generic host template — useless for naming a writer. Added
`trace_addr_range_write` at the `guest_bridge.cpp` level instead (same
`AC6_DEMO_WATCH_ADDR_LO`/`HI` env vars), where `tick`/`thread`/`lr`/
`generated_name` are already in hand for every scalar store
(`AC6_PPC_STORE_U8/U16/U32/U64`) — the same context `trace_ib_write`
already uses. Two lines per matching write is not elegant; it is much
cheaper than plumbing `PPCContext` through `GuestMemory`.

Watching `[startup_this+12]` (`0x2E7E008C`) across the full run named the
exact writer immediately: `value=0x2 tick=2425 thread=1 lr=0x821728E8
function=__imp__sub_821728C0`.

## The chain, traced hop by hop

1. **`sub_821728C0`** — the previously-unread real function at slot `+0x54`
   of the task's *listener* vtable (the same "extra slot the message
   dispatcher doesn't use" `69ff833a` found and deprioritized). Its body:
   adjust `this` back to the primary object (`this - 104`), call primary
   vtable slot `+0x48`, then **unconditionally** `stw 2, this+12` — no
   condition at all, this function's entire job is "signal complete."
2. **Who calls it**: the atlas's `indirect_edges` show it called exactly
   once in the whole 12000-tick run, from `lr=0x820EA500` — inside
   **`sub_820EA4A8`**, which loops over the listener array at `0x826DF800`
   (`SendMsgI`'s own array — confirmed by address, `0x826DF800`) calling
   **slot `+0x54`** on each populated entry, guarded only by `sub_820CE010`
   ("does any populated slot exist from here on" — trivially true whenever
   any listener is registered, which is always: listener `[0]` is a
   permanent fixture).
3. **Who calls `sub_820EA4A8`**: no direct `bl` caller anywhere in the
   codebase; the atlas's `indirect_edges` show it too is called exactly
   once, from `lr=0x820E9130` — a `bctrl` return address **inside
   `sub_820E8F90`**, the ActionScript/swg native-call marshalling
   dispatcher a much earlier, unpublished note in this campaign's memory
   already found (`AC6_DEMO_THE_SCRIPT_VOCABULARY.md`'s subject).
4. **`sub_820E8F90`'s own reachability**: the atlas confirms it genuinely
   runs — `first_tick=1045, last_tick=3001, count=9`. **This corrects a
   stale memory note** that called this dispatcher's only reference "an
   unreached no-op" — that described the table's address being passed as
   *data* to a different, unreached function; the dispatcher itself is a
   live, repeatedly-invoked native-call site, reached from the swg script
   interpreter as its movie plays.

## The full picture

```
swg script (startup's boot-logo movie) reaches a scripted cue
  -> sub_820E8F90 (the native-call marshaller, reached 9x, ticks 1045-3001)
     marshals and invokes a native command at some cue
       -> sub_820EA4A8 (listener-array pump, slot +0x54, reached once)
          -> sub_821728C0 (startup's own listener's +0x54 handler)
             -> unconditionally writes startup_this+12 = 2   (tick 2425)
                -> state 2's countdown fires, manager->request = 1
                   -> mode manager switches startup -> title (tick 2429)
```

Title's own listener has the identical, real, un-inspected `+0x54` slot
(`0x8217C890`, per `69ff833a`'s dump) — structurally the same mechanism is
available to title. **Directly confirmed unreached** in the same atlas
(absent from `functions[]`, no `indirect_edges` entry targeting it) — not
inferred from the pump's own call count.

## Correcting my own first draft: the flag does NOT gate this

My first draft of this report claimed title's movie "never plays a single
frame" because `CSwgCallback+9` is dead, and concluded the whole
investigation "converges on one root byte." **That is contradicted by this
session's own prior measurement.** `fbd898c1` watched *startup's* flag
across its *entire* active window (tick 266-2426) and found the identical
dead pattern — construction only, never touched again — yet startup's cue
fired anyway at tick 2425, inside that same window. Script execution
reaching a native-call cue and the flag-gated frame/render step
(`fcecb736`: increments `[player+4132]`, calls `player.vtable+68`) are
**demonstrably independent** — startup completed its cue with zero frame
steps and a permanently-zero flag. And title's own script is not idle
either: `eab92d66` recorded 31 `swg::ASContext` dispatches at tick 3001,
and this atlas's `sub_820E8F90` reachability (`first_tick=1045,
last_tick=3001`) spans into title's own tenure — its movie is running.

**Corrected conclusion**: `CSwgCallback+9` gates only the per-frame
counter/render step, not script progression. The real discriminator for
why title never reaches the `sub_820E8F90` → `sub_820EA4A8` → `+0x54` chain
is a **script-content/script-decision** question — which native command,
if any, title's own movie is scripted to invoke, and whether it ever
issues it. This reopens `3c7e7291`'s thread (all three script accessors —
`GetCurrentMode`/`GetCurrentMission`/`GetCurrentLevel` — return constant
fallback values regardless of START) as directly relevant again: if
title's script branches on one of those queries before deciding whether to
issue the completion command, a wrong/fallback answer would explain
exactly this — script running, cue never reached, without needing
`CSwgCallback+9` to be involved at all.

## Not established

- What native command, specifically, `sub_820E8F90` marshals at each of
  its 9 calls, and which (if any) is the one that resolves to
  `sub_820EA4A8` for startup — not identified. `sub_820E8F90` is called
  only 9 times in 12000 ticks, so instrumenting its per-call tag/arguments
  directly is cheap and is the concrete next step.
- What title's own script asks for or attempts at tick 3001 (where its own
  `ASContext` dispatches were recorded) instead of issuing the completion
  command — not compared against startup's sequence.
- Whether `0x8217C890` (title's own `+0x54` handler) is structurally
  identical to `sub_821728C0` — plausible by neighborhood and pattern, not
  read; moot until something is found that would call it.
- What, if anything, would ever set `CSwgCallback+9` — `956bd743`'s
  exhaustive rule-out stands, but per the correction above it no longer
  looks like the load-bearing question for title's stall; it may matter
  only for visible rendering, not for state progression.

## Gates

`AC6_DEMO_WATCH_ADDR_LO`/`HI` in `guest_bridge.cpp` is a new, opt-in-only
addition (two env vars, unset by default; adds two trace calls per scalar
store, each an immediate no-op return when unset). Demo ctest, native gate
JF, and both contract audits all pass.
