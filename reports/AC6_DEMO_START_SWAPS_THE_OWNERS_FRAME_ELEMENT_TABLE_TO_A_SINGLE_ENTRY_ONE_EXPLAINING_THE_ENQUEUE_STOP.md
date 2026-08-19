# START swaps the dominant owner's frame-element table to a single entry,
# fully explaining why the enqueue call site stops firing

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Re-analysis of the already-completed probe log
from `1a9b432c` (`--until frontend --max-ticks 3600 --input-at
3000,16,0,0,0,0,0,0,1 --input-at 3001,0,0,0,0,0,0,0,1`,
`AC6_DEMO_WATCH_ADDR_LO/HI` bracketing `[0x2E4035D0, 0x2E403830)`) — no new
probe run needed, plus static reads of `sub_82323BB8`
(`ppc_recomp.43.cpp:16291`, its dispatch loop at `loc_82323E30`,
lines 16552-16685). No source change.

## What this closes

`1a9b432c` precisely localized the stop to one function,
`sub_82322A80` (the script-statement-enqueue cursor-walker, `704b27b6`),
ceasing to fire at tick 3000/3001 while its owning object stayed alive and
active. It left open *why* the type-dispatch branch selecting
`sub_82322A80` stops being chosen. This report reads the actual dispatch
loop and re-mines the already-collected trace for the answer — no new
live run required, the data was already captured.

## The dispatch mechanism, read in full

`sub_82323BB8`'s loop at `loc_82323E30` runs `[owner+44]` iterations
(after the modulo-wrapped index computation below), each iteration:

```
type       = [[owner+40] + index*8]           // per-element type tag
table[type*4]                                    // -> the third jump table,
                                                    0x8264CE6C, containing
                                                    sub_82322A80 at slot 4
bctrl                                                // dispatch
```

`index` (`[owner+220]`) is either the running frame counter
(`[owner+216]`, incremented every call, wrapping at `([owner+44] -
[owner+40]) >> 3` — the element count) or forced to `0`, selected by a
one-byte flag at `[owner+212]`. `[owner+40]`/`[owner+44]` are the
frame-element table's own start/end pointers — **not fixed**, reassigned
by a separate function, `sub_82326B80` (`lr=0x82323860`), every time this
owner's active frame changes.

## The live data: the table itself gets swapped at the press, from 2 entries to 1

Re-reading the existing bracket log (no new probe) for
`[owner+40]`/`[owner+44]` (`0x2E4035F8`/`0x2E4035FC`):

```
tick 2571 (owner becomes dominant): table = [0x2DD69710, 0x2DD69720)
                                      -> 0x10 bytes = 2 entries (8B stride)
tick 3001 (the press):               table = [0x2DD6A854, 0x2DD6A85C)
                                      -> 0x08 bytes = 1 entry
```

Both writes come from the identical call site (`sub_82326B80`,
`lr=0x82323860`) at both ticks — the same "load this owner's active
frame's element table" operation, invoked once at construction/activation
and once again, with different data, exactly at the press.

**The index field (`[owner+220]`) confirms the consequence directly**:
before the press it alternates `0`/`1` every tick (matching the 2-entry
table). From tick 3002 onward, sampled continuously through the run's
end, it is `0` on every single write, with zero exceptions — exactly what
a 1-entry table's modulo wraparound forces. `sub_82322A80` stops firing
not because anything broke, but because **the one remaining element in
the new, smaller table apparently does not carry the type tag that routes
to it** (confirmed behaviourally: the dispatch loop keeps running every
tick — the broader bracket shows continuous activity through tick 3599 —
it simply never selects `sub_82322A80`'s slot again).

## Reading

**This is a complete, address-level mechanism, not a hypothesis.** START
causes this owner (a `MovieController`-family object, `a42271ec`) to swap
its active per-frame element table for a different, shorter one — the
same operation, on the same call path, that presumably runs on every
ordinary frame transition throughout the attract loop, just landing on
different content this one time because of the button input. The
element that used to enqueue script calls (walking a cursor that would,
over enough attract-loop cycles, eventually enqueue `menu_endMode`) is
simply absent from whatever this new one-element table represents. This
directly and completely explains `1a9b432c`'s observed 857→2→0 pattern
with no remaining gap in the causal chain from "button pressed" to
"enqueue stops."

**What this does not yet establish**: whether this is a *bug* (the new
frame's data is supposed to include a script-enqueue element, and it's
missing or misidentified) or *intended design* (the new frame is a
transition/confirm state whose own single element is meant to do
something else — trigger a *different* mechanism entirely, e.g. directly
invoke a native call, load a new movie, or hand off to a completely
separate `CModeTask`-family object — and the demo's actual bug, if any,
is downstream of *that*, not in this table swap itself). The addresses
involved (`0x2DD69710`/`0x2DD6A854`) sit in the same numeric neighborhood
(`0x2Dxxxxxx`) this campaign has already found holding large,
background-loader-filled compiled data blobs (the `swg` statement buffers
at `0x2DCBxxxx`, `346255b2`) — consistent with this being pre-authored
movie/timeline content, not something the port's own code constructs, but
not confirmed to be the same kind of blob.

## Not established

- The actual 8-byte content of the new table entry at `0x2DD6A854` — what
  type tag it carries, and what (if anything) it's supposed to dispatch
  to. This is the direct next read: a live memory dump of that address at
  or after tick 3001 (this region was outside the bracket this report
  reused; a fresh, narrow bracket on `[0x2DD6A854, 0x2DD6A85C)` would
  read it in one more short run).
- What determines the argument `sub_82326B80` is called with (i.e. what
  selects "frame index → new table address" at the press) — not traced;
  this function's own body was not read in this report.
- Whether this exact mechanism (frame-table swap on button press) is
  unique to this owner, or a generic pattern every `MovieController`-
  family object goes through on any state transition — not checked
  against `e4e9b251`'s owner (`0x2E3EDA90`), whose own frame-table fields
  (if the same offsets apply) were not read in this report or `e4e9b251`.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change, no new probe run — this report
re-analyzes an already-collected trace plus new static reads.
