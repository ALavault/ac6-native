# `Count()`/`GetAt()` read a separate, stable pointer pair (`+28`/`+32`) untouched by the buffer swap `a42271ec` found

## Qualification

AC6 demo PAL, same XEX SHA-256. Entirely static: full reads of `MovieMemory`'s
vtable (`0x8200654C`, already dumped in `a42271ec`) slots 1-5 and slots 10-11,
plus a raw-word occurrence count confirming each is a genuine, singly-referenced
vtable entry rather than dead code. No probe run.

## What this refines

`a42271ec` read the write pattern of `MovieMemory`'s slot-1 method (`+20`/`+24`
swapping once per tick) but explicitly left the method's own body, and
`Count()`/`GetAt()`'s own bodies, unread — flagging "looks like a
producer/consumer double buffer" as a reading of addresses, not a
disassembly. Reading all of it now shows the swap is real, but **it is not
what `Count()`/`GetAt()` read from.**

**Slot 1 (`sub_820D0F20`), the swap, in full** — eight real instructions,
followed by a tail call:

```
r10 = [this+8]              // the "0x5A" (90) field a42271ec left unidentified
r9  = [this+24]               // save old +24
r5  = r10 << 2                 // capacity * 4 (byte count)
r10 = [this+20]                 // save old +20
r3  = r9
[this+20] = r9                   // new +20 = old +24
[this+24] = r10                   // new +24 = old +20
tailcall sub_823273E0(r3=old_+24_value, r4=0, r5=capacity*4)
```

`sub_823273E0` is the same function `bfc927e1`/`a42271ec` already identified
as the debug pool-fill routine (`0xFEFEFEFE` poison, seen at every object's
construction this whole session) — here called with fill value `0`, i.e. a
plain `memset`. Read together: swap `+20`⟷`+24`, then **zero the buffer that
just became the new `+20`**, sized by `[this+8]` (90) words. This confirms
`a42271ec`'s "looks like a swap" reading was correct.

**But `Count()` and `GetAt()` — the two methods `6d61b5cd` actually observed
dispatched live during the per-tick queue drain — read neither `+20` nor
`+24`:**

```
Count()  (vtable slot 10, sub_82358FD0):  return [this+32]
GetAt(i) (vtable slot 11, sub_820D0FF8):  return [[this+28] + i*4]
```

A **third, separate pointer field, `+28`**, is what the live queue-drain
actually indexes into — a field `a42271ec` did capture at construction
(`0x2E3EE1D4`) but treated as "a third pointer, unread." `Count()`'s own
source, `+32`, falls outside `a42271ec`'s bracket (`[0x2E3EDCD0,
0x2E3EDCF0)` covers only `+0` through `+28`) and has never been captured.

Slots 2-5 (`sub_820D0F48/F58/F68/F78`) — the indexed getters/setter on
`+20`/`+24` read in this same pass — are confirmed live, referenced
vtable entries (each appears exactly once as a raw word in the image, at
its own vtable slot, not dead code), but nothing yet connects them to the
`+28`/`+32` pair the drain loop actually uses. **The swap `a42271ec`
tracked and the queue the drain loop reads appear to be two different
mechanisms on the same object, not one.**

## What this means for "who enqueues"

`a42271ec` framed the open question as finding the writer that fills the
`+20`/`+24` pair between swaps. That framing no longer holds: `+20`/`+24`
is not what `GetAt` reads, so a writer there would not be visible to the
drain loop at all. The real target is **`+28`** (the array `GetAt` indexes)
and **`+32`** (the count `Count()` returns) — neither has been bracketed
live yet.

## Not established

- What writes `+28`/`+32`, and when relative to a tick — not bracketed
  live. This is now the direct next step: point the existing
  `AC6_DEMO_WATCH_ADDR_LO/HI` instrument at `[collection+28,
  collection+36)` for one of the two named `MovieController` instances
  and read the write log the same way every prior step in this arc has.
- Whether slots 2-5's `+20`/`+24` pair (confirmed live/referenced, swapped
  once per tick) serves any purpose connected to the drain queue at all,
  or is an unrelated mechanism on the same class (a second, independent
  use of `MovieMemory` — e.g. transform/animation double-buffering, given
  `sub_82323BB8`'s own Phase 1 does per-tick transform math on a sibling
  field cluster) — not traced either way.
- `[this+8]` (`0x5A` = 90) as the swap's clear size in words — still not
  confirmed as a capacity bound on the `+28`/`+32` pair specifically,
  since it's read by the swap method, not by `Count`/`GetAt`.

## Gates

No source changed; this report is entirely static (vtable slot dump plus
five function bodies plus a raw-occurrence check). Native gate JF, demo
`ctest` (26/26), and both contract audits verified below before commit.
