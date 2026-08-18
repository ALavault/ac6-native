# `category` is the native-function ID; title never evaluates category 1, confirmed durable to tick 8000

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `probe --until frontend`,
headless backend, no oracle, correctly-timed START (tick 3000). Two runs:
a 3200-tick run correlating `AC6_DEMO_WATCH_SWG_LOOKUP_KEY` against the
existing `AC6_DEMO_WATCH_SWG_NATIVE_CALL` in the same stream, and an
8000-tick extended-window rerun testing whether the apparent post-press
silence was real or an artifact of the first run's tick window.

## What this closes

`AC6_DEMO_THE_SYMBOL_TABLE_IS_AN_MSVC_MAP_KEYED_BY_TWO_INTEGERS.md`
(`a015f994`) named the next step as live: log the `(category, id)` pairs
the AST-node evaluator (`sub_820DFFB8`) actually looks up. This report runs
that, correlates it directly against which native function fires, and then
tests (per advisor guidance) whether an initial 9-tick post-press gap in
the lookup trace was a real "the driver stopped" finding or a window
artifact.

## The correlation

Running `AC6_DEMO_WATCH_SWG_LOOKUP_KEY` and `AC6_DEMO_WATCH_SWG_NATIVE_CALL`
in the same run (both write to the same `stderr`, in call order) makes the
pairing direct — no timestamp matching needed, a lookup line is
immediately followed by its own resulting call line when the lookup
resolves to a `type==2` (native-call) node:

```
category=0x00000001                    -> target=0x820EA4A8 (tick 2425, startup's completion trigger)
category=0x00000019 (25)               -> target=0x820EA0A8 (tick 2452)
category=0x00000004                    -> target=0x820EA598 (tick 3001, GetCurrentLevel)
category=0x00000005                    -> target=0x820EA550 (tick 3001, GetCurrentMission)
category=0x00000006                    -> target=0x820EA538 (tick 3001, GetCurrentMode)
category=0x00000017 (23)               -> target=0x820E9838 (tick 3001, SendMsgI, tag=M102)
category=0x0000000B (11)               -> (no call follows -- a non-native-call node)
category=0x00000013 (19)               -> target=0x820EA6C0 (tick 3001, NUD_TONE_BANK)
```

Every lookup that resolves to a call names that exact call's target,
1:1, zero mismatches across all 9 marshaller invocations this campaign has
ever observed. **`category` is the native function's own ordinal ID in the
symbol table** — a direct, unambiguous confirmation of
`a015f994`'s structural reading. `id` is `0` in every observed lookup
(unused in this script, or reserved for a dimension this campaign hasn't
exercised). Checked against `a015f994`'s registration-order guess (linear
indexing off the command-table rows): it does not hold —
`GetCurrentLevel`/`Mission`/`Mode` (4/5/6) are consecutive, but `SendMsgI`
(23) and the completion trigger (1) break any single linear stride from
the command-table addresses. The map is a real map with independent
registration, not an array view of that table.

**Title's tick-3001 response evaluates exactly six symbol-table lookups,
categories `{4, 5, 6, 11, 19, 23}`. Category `1` — the ID that resolves to
the completion trigger — is not among them.**

## The silence question, resolved: the driver never stops, the response batch is one-shot

The first (3200-tick) run's lookup window ended at tick 3010, 9 ticks past
the press, and showed nothing after the six-lookup batch — reading as
"the driver stopped issuing lookups entirely." Extended to 8000 ticks
(5000 ticks post-press) with the window opened through the whole run:
**this reading was wrong, and was a window artifact.** The driver
(`lr=0x820D452C` for every lookup, in every run) keeps firing every single
tick, continuously, through tick 7999 — the last tick before the run ends.
4985 total lookups recorded; all but the six in the tick-3001 batch (and
two before it, at ticks 2425/2452) are the same recurring per-tick node,
`category=0x2F` (47) — evaluated once a tick, uninterrupted, both before
and after the press. **The correct reading: the driver runs continuously
and never stops; title's multi-statement response to START executes
exactly once (tick 3001) and is never re-entered; from tick 3002 onward,
through the full 5000-tick extended window, the evaluator returns to
lookup up only the routine per-frame node and never again evaluates any of
`{1, 4, 5, 6, 11, 19, 23}`.**

## Reading

This narrows, but does not close, `AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS.md`'s
standing (a)/(b) split on why nothing advances. What's now established:
title's script, in the one AST subtree it evaluates immediately after
START, contains exactly six native-function references, and completion
(category 1) is not one of them — not because of a runtime branch a wrong
value could redirect (which is exactly why three separate single-value
forcing experiments, `c73498cb` and `6fc7b184`, found nothing to change),
but because the evaluated statement list simply does not reference that
symbol. **What this run cannot distinguish, and what the instrument
structurally cannot answer**: whether category 1 is genuinely absent from
title's compiled script data anywhere (this campaign's option (a)), or
present but gated behind a condition/subtree this input sequence never
causes the driver to reach (option (b)). This instrument only sees what's
*evaluated*, never what exists unevaluated in the script's own data — the
discriminator advisor named is finding the map's own *registration* site
(who inserts `category -> descriptor` pairs, and when), which would show
whether title's script ever references category 1 at all, independent of
runtime evaluation.

## Not established

- What `category=0x0B` (11) resolves to — a real symbol-table entry whose
  node's type is not `2` (not a native call); its actual role (a variable
  read, a different statement kind) is unread.
- What `category=0x19` (25, tick 2452's call) or `category=1`'s node data
  actually contain beyond their type tag — not dumped.
- Whether `id` (always `0` here) is ever nonzero for some other symbol,
  and what dimension it would then distinguish.
- The map's registration site — who inserts entries, and whether category
  1 is registered at all for title's script object, independent of
  whether it's ever looked up. This is the concrete next read named above.
- What drives `lr=0x820D452C` itself, and specifically what decides which
  AST subtree it evaluates on a given tick (the routine per-frame node vs.
  the one-shot response batch) — the actual dispatch/statement-sequencing
  logic, one level up from everything traced so far.

## Gates

Instrument change: widened `AC6_DEMO_WATCH_SWG_LOOKUP_KEY`'s tick gate from
two ~60-tick windows to a single `[2990, 8000]` range, matching this
report's own extended-window test — same env var, same call site, no new
line added to `AC6_PPC_CALL_INDIRECT`. Native gate JF, demo `ctest`
(26/26), and both contract audits verified below before commit.
