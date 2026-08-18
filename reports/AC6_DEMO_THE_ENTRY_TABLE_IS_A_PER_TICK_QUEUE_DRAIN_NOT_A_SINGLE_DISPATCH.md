# The per-tick entry table is fed by a queue-drain loop, not a single dispatch — and one confirmed yield/resume spans the press

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run: entirely re-analysis of
`b67e7f6f`'s own PC-field bracket log
(`AC6_DEMO_WATCH_ADDR_LO/HI=0x2E3DFA1C/0x2E3DFA20`). Static:
`sub_823251E0`'s body (already partially read in `b67e7f6f`, re-examined
here), `sub_82325288`/`sub_82325298` (its two known callers), and
`sub_82323BB8` (the caller reached through `sub_82325288`'s tail-branch),
`ppc_recomp.43/44.cpp`.

## Correction to `b67e7f6f`: not one write per tick, and not one stable caller

`b67e7f6f` described `sub_823251E0`'s `generated_line=1133` write as
firing once per tick from "a stable, single caller." Both parts need
narrowing:

- **Writes per tick, tabulated exactly**: 576 of 602 ticks show **two**
  entry-setter writes, not one; 11 ticks show three, 4 show four, 8 show
  five, and only 3 ticks show exactly one. `b67e7f6f`'s own 38-value
  table used first-write-per-tick only (`setdefault`), which is still
  correct for naming *an* entry each tick, but roughly a third of the 38
  values only ever occur as a **second or later** write within some tick,
  not a first. Multi-event-per-tick dispatch is a fact, not an inference.
- **The call site is a tail-branch, not a distinct caller.** The direct
  `bl` target at `lr=0x82323F2C` is `sub_82325288` — a four-instruction
  thunk that stores its own `r5`/`0` into the "this" object's `+4`/`+8`
  fields, then reaches `sub_823251E0` via `b` (unconditional branch, not
  `bl`), so `lr` is never overwritten and the trace's `lr=0x82323F2C`
  correctly names `sub_82325288`'s own caller, one level further up. A
  **sibling thunk, `sub_82325298`**, sets up the same two fields
  differently (`this+8=r5`, `this+4=[r5+32]`) before the same tail-branch
  — a second, statically-confirmed entry path into `sub_823251E0` that
  this run never exercised. Other paths may exist; not swept.

## The mechanism, read precisely

`sub_823251E0`'s own computation (`ppc_recomp.44.cpp:1083+`, the part
this report and `b67e7f6f` between them have read — the `loc_82325274`
early-exit branch and anything past this point are still unread):

```
table_base = [[this+0]+8]     // also stored to this+16
pc_field   = table_base + r4  // this+20 -- r4 is sub_823251E0's OWN 2nd argument
```

**Selection does not happen inside `sub_823251E0` at all** — it is a
plain base-plus-offset computation. The offset (`r4`) comes from its
caller, `sub_82323BB8`, which loops (`r29` from some start, bounded by
`r28`) over what reads as a **per-tick queue**: each iteration calls two
virtual methods on a per-item object (`r30`) — one at vtable **slot 11**
(`[r30]+44`, note: slot 11, not "slot 44" — `+44` bytes is slot 11 at
4 bytes/slot; `+44` is also this campaign's marshaller slot number by
coincidence of a *different* class's vtable, not the same method) whose
result (`r22`) becomes the offset passed into the thunk, then one at slot
8 (`[r30]+32`) — before calling the thunk with `r4=r22`. **"What selects
each tick's entry" reduces to "what items are enqueued, and who enqueues
them"** — this report does not reach that; `sub_82323BB8`'s own queue
(`r28`, `r29`, and where the collection itself lives) is unread.

## One confirmed yield/resume across the press; not a universal pattern

Tick 3001's **last** PC-field write of any kind (`sub_82325160`'s own
fetch-advance, not the entry-setter) is `0x2DCB268C` — **exactly**
tick 3033's entry-setter value. Between them, the title interpreter
instance produces **zero** events for 31 straight ticks (3002-3032): the
signature of an item that yielded mid-execution and was re-queued,
resuming from the exact PC it left off at, 31 ticks later. **This does
not generalize**: tick 3033's own last write (`0x2DCB293C`) does *not*
match tick 3034's first entry (`0x2DCB2124`), and tick 3034's last write
(`0x2DCB18B4`) does not match tick 3035's entry (`0x2DCB2130`) either —
those two transitions are fresh dispatches, not resumes. One clean
resume, two ordinary re-dispatches, from three checked boundaries.

Tick 3001 itself runs **two** entry-setter dispatches, not one:
`0x2DCB2430` (the six-lookup batch, already characterized) and
`0x2DCB264C`, landing between the batch's `box(0xB)` (fails to resolve)
and `box(0x12)` — shortly after the batch's own `SendMsgI` call
(`box(0x17)` at `0x2DCB256C`). **Offered as a timing observation, not a
finding**: this is consistent with the still-unanswered `M102` query
(`AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS`) itself
being one queued item — SendMsgI enqueues a message, the message is
drained as a second same-tick dispatch. Not verified; no evidence beyond
the one timing coincidence.

## What this leaves standing from `b67e7f6f`

The EndMode-address gap (no entry-setter value ever lands inside
`[0x2DCB2024, 0x2DCB2044)`) is unaffected by any of the above — it holds
whether dispatches are single or multiple per tick, and whether a given
dispatch is a fresh entry or a resumed one: EndMode's statement has never
been **dispatched into** (fresh) and never been **resumed into** either,
across every entry-setter write in the whole run.

## Not established

- `sub_82323BB8`'s own queue: what `r28`/`r29` count, where the item
  collection lives, who enqueues an item and when. This is the actual
  next static step — the shape to expect is an enqueue call reachable
  from wherever `SendMsgI`/other events originate, per this campaign's
  own prior work on that subsystem.
- `sub_823251E0`'s `loc_82325274` early-exit path, and its body past the
  table-base computation (the conditional virtual call at the very end of
  the excerpt in `346255b2`/`b67e7f6f`) — read in part, not in full.
- Whether any enqueueable item can ever carry an offset landing inside
  EndMode's gap — not determined; requires the queue's own contents, not
  more PC-field bracketing.
- `sub_82325298`'s own callers (the second, unexercised thunk path) — not
  traced.

## Gates

No source changed; report-only commit, entirely re-analysis of a bracket
already tested and committed in `b67e7f6f`.
