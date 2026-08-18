# The enqueue call is `Add()` on `MovieMemory`, driven by a tiny cursor-walker that is itself one of Phase 1's own type-dispatched handlers

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` bracket (no source change) widened to
`[collection+24, collection+40)` on the press/resume `MovieController`'s
own `MovieMemory` object (`0x2E3EDCD0`, named in `a42271ec`), `probe
--until frontend --max-ticks 3200`, correctly-timed START, no oracle.
Static: full reads of `MovieMemory`'s vtable slots 9 and 12
(`42731207`'s open question), the function that calls slot 9, and a raw
word scan locating that function inside a third static jump table.

## The real `Add()`, read in full

`42731207` named `+28`/`+32` as the fields the live-dispatched
`Count()`/`GetAt()` actually read, and left "what writes them" open.
Vtable slot 9 (`sub_820D0FD8`) is the answer — `Add(item)`:

```
r10 = [this+32]         // current count
r11 = [this+28]          // array base (fixed, see below)
[r11 + r10*4] = r4          // array[count] = item  (r4 is the caller's own arg)
[this+32] = [this+32] + 1     // count += 1
```

Slot 12 (`sub_820D1008`) is `Clear()`: `[this+32] = 0`, nothing else — it
does not touch the array or its pointer, which is why `42731207` found
`+28` written exactly once, at construction (`0x2E3EE1D4`), and never
again in the whole 3200-tick run: `Clear()` only resets the count, so
`Add()` after a `Clear()` overwrites from index 0 again. The array is a
fixed 90-word buffer (`[this+8]`, `a42271ec`), reused in place every tick.

Live, widening the bracket to cover `+32` across the full run: **`Add()`
fires on 573 of 750 sampled ticks** (`value=0x1` written 573 times),
**twice in the same tick on exactly 2 ticks** (`value=0x2`), and `Clear()`
fires once per `Add()`-touched tick, restoring `0`. This matches
`390cfe33`'s much earlier count of the entry-setter firing "twice on 576
of 602 ticks" almost exactly — the two findings, five reports apart and
reached by entirely different instruments, describe the same underlying
event.

## The caller: a two-instruction cursor walk, unconditional

Slot 9's only live caller in this run, `lr=0x82322AB4`, is
`sub_82322A80` (`ppc_recomp.43.cpp:13558-13606`), read here in full — the
*entire* function body, not an excerpt:

```
this = r3
collection = [this+16]        // the MovieMemory this tick's item belongs to
cursor      = [this+248]        // a cursor into some other, 8-byte-stride list
value        = [cursor+4]          // the payload
Add(collection, value)               // <- the enqueue
[this+248] = cursor + 8                // advance to the next entry
```

No loop, no condition — this function processes exactly one cursor entry
per call and always calls `Add()`. Whatever decides *whether* to call
this function, and what populates the 8-byte entries `[this+248]` walks,
lives in this function's own caller, not here.

## The caller is one of Phase 1's own type-dispatched handlers — correcting `6d61b5cd`

`6d61b5cd` read `sub_82323BB8`'s Phase 1 (the "current item" dispatch
through two static jump tables, `0x8264CDC0`/`0x8264CDF8`) and called it
explicitly "**unrelated to script dispatch**... the clip's own animation
update." A raw scan for `sub_82322A80`'s own address as a data word finds
it inside a **third** table, immediately adjacent to those same two,
same 14-slot layout, same code neighbourhood:

```
0x8264CE6C: 0x82323968 (repeated x4)
0x8264CE7C: 0x823239F0
0x8264CE80: 0x82323A70
0x8264CE84: 0x82322A80   <- our function, the enqueue-cursor-walker
0x8264CE88: 0x82323220
0x8264CE8C: 0x823232B8
0x8264CE90: 0x82323338
0x8264CE94: 0x82322AD8   (repeated x4, the table's tail padding)
```

**This directly corrects `6d61b5cd`'s "unrelated to script dispatch"
characterization of Phase 1.** Phase 1 is not purely a visual
animation-update dispatch: at least one of its own type-dispatched
branches (whichever "type" value selects this third table's slot
containing `0x82322A80`) *is* the mechanism that drives events into the
script queue. Script dispatch and "the clip's own animation update" are
not two separate systems downstream of title — enqueuing is one branch of
the same per-element type dispatch the animation update already uses.

## What this means for "could EndMode's offset ever be enqueued"

The chain from clip element to script queue is now: Phase 1's own
per-element type dispatch → (for elements of the type that lands on this
third table's `0x82322A80` slot) → walk one 8-byte cursor entry → `Add()`
the entry's own `+4` value onto the target `MovieController`'s
`MovieMedmory`. Whether any cursor entry's `+4` value can ever equal
EndMode's offset (`0xE04`, `0x2DCB2024 - 0x2DCB1220`, per `advisor`'s
computation from `bfc927e1`'s `table_base`) is a question about **what
populates the cursor list `[this+248]` points into** — not yet read.

## Not established

- **Table 3's own dispatch site and selector** — nothing here confirms
  *what* calls through this third table, with what index, or whether it
  uses the same "type" value Phase 1's first two tables use. The table's
  mere adjacency and matching layout is strong circumstantial placement,
  not a traced call.
- **The cursor list** (`[this+248]`, 8-byte entries, value at `+4`) — its
  own source, size, and who populates it (and when) are all unread. This
  is where "who decides what gets enqueued" is actually answered.
- **A second caller of `sub_82322A80`**, `0x820856C8`, found in the same
  raw scan, in an unrelated code region (`0x8208xxxx`) — not examined.
- Whether `Add()`'s two-in-one-tick occurrences (2 of 750 sampled ticks)
  come from two different cursor walks, two elements sharing one type, or
  something else — not distinguished.

## Gates

No source changed; this report reuses the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` instrument on a wider bracket, plus static
reads. Native gate JF, demo `ctest` (26/26), and both contract audits
verified below before commit.
