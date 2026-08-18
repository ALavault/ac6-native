# The dominant owner's array also cross-validates a prior finding exactly, stays clear of EndMode, and is torn down at the press

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: two chained probe runs,
both the existing `AC6_DEMO_WATCH_ADDR_LO/HI` bracket (no source change)
— first the dominant owner's (`0x2E4035D0`, `a42271ec`) collection header
(`0x2E403810`) to find its array pointer, then the array itself
(`0x2E403994`) — `probe --until frontend --max-ticks 3200`,
correctly-timed START, no oracle.

## The dominant owner's own array, cross-validated again

`e4e9b251` checked the press/resume owner's (`0x2E3EDA90`) array and
found two exact matches to prior, independently-derived addresses. This
report checks the *other* named owner, the dominant per-tick one
(`0x2E4035D0`), the same way. Its collection object (`0x2E403810`) has
the same class (`0x8200654C`, `swg::MovieMemory`) but a different
capacity field (`[+8]=0x1`, versus owner A's `0x5A`) and its own array,
`0x2E403994`.

From tick 2571 through 3000 — the exact 429/430-tick idle window
`b67e7f6f` originally found this owner active in — **the array
alternates every tick between two values**: `0xE94` and `0xE88`.
Converted through `table_base` (`0x2DCB1220`):

| array value | absolute address | reading |
|---|---|---|
| `0xE94` | `0x2DCB20B4` | **exact match to `b67e7f6f`'s "dominant entry `0x2DCB20B4`, 429 consecutive ticks 2572-3000"** — the report that first found this owner's idle behavior, four commits before this one, from a completely different bracket |
| `0xE88` | `0x2DCB20A8` | not previously named — the alternating partner |

A third independent measurement lands on an address this campaign
already named from a fourth angle: this is the second time in two
reports that `Add()`'s own raw write log reproduces a prior finding
exactly, on a different owner, via a different bracket. **Neither value
equals EndMode's own offset** (`0xE04` → `0x2DCB2024`) — both sit `0x84`
and `0x90` bytes past it, close by this buffer's scale but never on it.

## The array is torn down at the press — tick 3001's writes are heap reuse, not new enqueues

At tick 3001 the bracket keeps firing, but the writer changes: every
write from `0x2E403994` after tick 3000 comes from `sub_820D4A30` and
`sub_820D4C38` — the **generic value-constructor and vtable-installer
pair** `346255b2` identified at the very start of this whole
investigation arc (the `ASContext::String` boxing chain), plus a handful
of sibling constructors (`sub_820D4CD8`, `sub_820DA8D0`, `sub_820D4650`,
`sub_820D3F20`) — **never `sub_820D0FD8` (`Add()`) again.** The values
written (`0x82006B44`, `0x820066F4`, `0x82006A9C`, `0x2E403890`,
`0x2E403AD0`) read as vtable pointers and fresh heap addresses, not
table offsets.

Read plainly: **the dominant owner's `MovieMemory` array is freed at the
press, and the allocator hands the same address to unrelated objects
immediately after** — the same heap-recycling pattern this campaign has
seen throughout (the 26+ owners sharing one execution-context slot,
`bfc927e1`; the poison-fill pattern at every construction). This is not
a new enqueue and not evidence about EndMode either way — it is an honest
gap: **what, if anything, this owner enqueues at or after the press is
unreadable from this address once it's been freed.** Whether the owner
itself is destroyed, or only this array is reallocated while the owner
persists, is not established.

## Standing picture

Both named owners now checked, same method, same result shape: every
offset either one is ever seen enqueueing, across the whole readable
history of each, corresponds either to a previously-known entry-table
address or a new-but-nearby value — and neither ever produces EndMode's
own offset. The negative result from `e4e9b251` is reinforced, not
weakened, by checking the second owner; the new caveat is that "readable
history" ends at the press for this particular owner, for a reason
(heap reuse) unrelated to EndMode.

## Not established

- Whether the dominant owner's `MovieController` itself is destroyed at
  the press, or only its `MovieMemory`/array specifically — not traced.
- What (if anything) replaces this owner's queueing role after tick 3001
  — not identified.
- The 24+ other owners `bfc927e1` found sharing the execution-context
  slot remain unchecked, as do other input sequences.

## Gates

No source changed; two live runs reused the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` instrument at different addresses. Native
gate JF, demo `ctest` (26/26), and both contract audits verified below
before commit.
