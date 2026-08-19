# Title's six tick-3001 symbols are a one-time burst; `EndMode` never
# appears, confirmed clean through tick 7999 on the fully natural route

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run, fully natural: `--until
frontend --max-ticks 8000 --input-at 3000,16,0,0,0,0,0,0,1 --input-at
3001,0,0,0,0,0,0,0,1` (`634cff33`'s correctly-timed START-during-title
tuple, no `AC6_DEMO_FORCE_*` overrides of any kind), neutral store,
`AC6_DEMO_WATCH_SWG_LOOKUP_KEY`/`AC6_DEMO_WATCH_SWG_NATIVE_CALL`. No source
change.

## Why this run, and what it corrects about the campaign's own current focus

A catch-up review of this repo's history (prompted by finishing
`7c833f03`, the `M150`/`EndMode` thread this whole session ran) established
that the entire `menu_endMode`-argument-forcing arc (`1edce620` through
`7c833f03`) investigates an **artificial hypothetical** — an argument value
(`1`) the game never naturally sends — not the actual natural-route
blocker. The natural route's own last-open thread
(`b29dcf77`/`1b87123e`/`29da1b05`) established that title's script, after a
correctly-timed START press, evaluates six local symbols once at
tick ~3001 and then appears to go silent, but every prior measurement of
that silence covered only a narrow window (`AC6_DEMO_CORRECTING_
CATEGORY_1...`'s own dump used `--max-ticks 3200`, ~200 ticks of coverage
past the press). This run applies the technique that just closed the `M150`
question — `AC6_DEMO_WATCH_SWG_LOOKUP_KEY`, unmodified — to the **natural**
route instead, for a full 5010-tick sampled window (the instrument's own
`[2990,8000]` range), to close that gap.

## Result: exactly one burst, six symbols, then total silence for 4998 ticks

```
category=0x03 (EndMode)        -- never appears, zero occurrences
category=0x04 (GetCurrentLevel)   tick=3001 only, 1 occurrence
category=0x05 (GetCurrentMission) tick=3001 only, 1 occurrence
category=0x06 (GetCurrentMode)    tick=3001 only, 1 occurrence
category=0x0B (unregistered, per 1e90f723/AC6_DEMO_CORRECTING_CATEGORY_1)
                                   tick=3001 only, 1 occurrence
category=0x13 (OnVoice2D)         tick=3001 only, 1 occurrence
category=0x17 (SendMsgI)          tick=3001 only, 1 occurrence
category=0x2F (unrelated context) tick=2990..7999, 4979 occurrences
```

Every one of title's own six symbols (`AC6_DEMO_CORRECTING_CATEGORY_1...`'s
established vocabulary for context `0x2E3EAA94`) fires **exactly once**,
all at the identical tick, then **never again** through the run's full
8000-tick horizon. `EndMode`'s own local index (`3`) is absent at every
single tick sampled — not merely absent from the original narrow window,
but absent across the entire natural-route run this campaign has tested to
date. `AC6_SWG_NATIVE_CALL` corroborates independently: 9 total calls in
the whole run, the last five all timestamped `tick=3001`, none after.

`category=0x2F`'s continuous per-tick firing (2990 through 7999, node
`0x2E3E86D0`) is a **different, unrelated context** — this exact node
address matches `0x2E3E7C94`'s own dumped symbol table (this session's
earlier `AC6_DEMO_SYMBOL_TABLE_MIN_TICK` run, `category=0x2F id=0x00000000
type_tag=0x00000002 value0=0x82007644 value8=0x00000001 value12=0x00000008`
— the "heartbeat" class `1e90f723` already identified as routing native
calls through a *different* vtable slot (`0x820E89B8`, not the marshaller
`0x820E8F90`), which is why it never produces an `AC6_SWG_NATIVE_CALL`
line regardless of how long the run continues. It is ordinary background
activity, not evidence about title's own dormancy.

## Reading

**Title's interpreter does not merely fail to select `EndMode`'s
segment — after the tick-3001 burst, it stops evaluating any local symbol
at all, for the rest of every natural run this campaign has tested.** This
is a stronger and more specific claim than `003daa94`'s "zero further
native calls": the AST-node evaluator (`0x820DFFB8`, the `lookup-key` call
site) fires for every evaluated expression, whether or not it ends in a
native call — variable reads, comparisons, anything the interpreter's own
statement-fetch loop dispatches through it. Its own complete silence for
title's context means title's per-tick re-entry (`b67e7f6f`'s finding)
either stops happening, or re-enters a segment that touches nothing this
call site sees — consistent with, and now the strongest available evidence
for, `b67e7f6f`'s framing that title's interpreter "never selects
`EndMode`'s segment," extended to "never selects anything requiring a
symbol lookup, after the one-time burst."

## Consequence for the plan

This closes the coverage gap the natural-route `EndMode` thread left open
when it was set aside (`29da1b05`: "the next candidate direction is not yet
named") — not by finding the missing direction, but by confirming, with
25x the previous window's coverage, that there is nothing later in time to
find on this exact route: whatever gates title's advance is decided (or
fails to be decided) once, at or immediately after tick 3001, not on a
delay this campaign hasn't waited long enough to observe. Combined with
`1b87123e`'s static finding (`EndMode`'s own category boxes cleanly when
directly injected, but its invoke step traps on a shared link-check,
`sub_820D5B90`, that no object in this campaign has ever been observed
satisfying), the honest state of the natural-route investigation is: title
is capable of calling `EndMode` in principle, its script never attempts to
during the one window it evaluates anything, and the deepest static read
of *why* it wouldn't (the six-word statement's own dispatch, the
`ASContext` list fields near it) explains generic VM plumbing but not a
specific missing precondition. `sub_820D5B90`'s own purpose — never named
beyond "a self-referential link-field validator," per `1b87123e`'s own
"Not established" — is the most concrete unresolved static thread; reading
it in full (not yet done by anyone) is the cheapest remaining lead inside
this subsystem. Failing that, per this campaign's own repeated experience
this session (three "bound but never evaluated" findings across `EndMode`,
message 102, and message 150), the honest recommendation is the same as
`7c833f03`'s: this specific script/`swg`-layer question has now been
examined about as thoroughly as static and live tracing can manage without
a new external data point (an oracle-side comparison of what the retail
game's own memory state looks like at this exact tick, if such a
comparison is affordable) — not a certified dead end, but a fourth
similarly-shaped investigation inside the same layer is low-expected-value
without one.

## Not established

- What `sub_820D5B90` actually validates, in engine terms — read only by
  instruction shape across three call sites, never by full body, by
  anyone.
- Whether waiting past tick 8000 (this campaign's longest run to date on
  this exact route) would ever show different behavior — extremely
  unlikely given the totally flat 4998-tick silence observed, but not
  literally proven impossible.
- Whether a *different* input (not the single START press this whole
  natural-route thread has exclusively used, e.g. a second press, a
  different button, or holding rather than tapping) would produce a
  different tick-3001-shaped burst that does include `EndMode`.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change — pure live trace with an existing,
unmodified instrument.
