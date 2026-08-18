# `0x2E3E3E38` is an `ASContext`'s own embedded list-head field; correcting `ddc49812`'s "boxing pool" framing

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Entirely static this cycle — reads against
`.build/Default.xex.base.bin` and
`recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.4.cpp`
and `ppc_recomp.6.cpp` (control-flow evidence, never copied), plus the RTTI
walk already run against the trace logs `ddc49812` produced (no new probe
run).

## What owns `0x2E3E3E38`

`ddc49812` left this open: `0x2E3E3E38`'s own `+0` field is never written by
guest code, so it reads as a field embedded in a larger, unidentified
structure rather than a freestanding object. Reading the one function that
writes its `+4` field — `sub_820CF958` — in full
(`ppc_recomp.4.cpp:10953-11008`) answers it directly:

```
sub_820CF958(this):                       // r3 -> r31
  r11 = sub_820CE750(12)                  // allocate 12 bytes
  if (r11 != 0):
    [r11+0] = r11                         // fresh node: self-link
    [r11+4] = r11                         // fresh node: self-link
  [this+4] = r11                          // install as this list's head
  [this+8] = 0                            // count = 0
  return this
```

This is a generic **empty intrusive-list initializer**: allocate a sentinel
node whose own `next`/`prev` point to itself (the standard empty-circular-list
pattern), then install it as `this`'s own head field. It never touches
`[this+0]` — that offset belongs to whatever precedes this list header in its
parent struct, not to the list itself, which is why the full-run bracket in
`ddc49812` found no write there at all.

`sub_820CF958` has exactly one generic caller shape, and one call site chain
sharing a single, larger constructor: `sub_820E1010`
(`ppc_recomp.6.cpp:878-994`). That constructor:

- installs its own vtable at `[r31+0]` = `0x820065A4` (RTTI-walked live, same
  method as every class name this campaign has used:
  `.?AV?$ASContext@V?$lwallocator@E$0A@$0EA@@stx@@@swg@@` —
  **`swg::ASContext<stx::lwallocator<char,0,64>>`, the execution-context class
  this whole investigation arc has been tracing** since title's and startup's
  own context slots were first named (`bfc927e1`, `346255b2`, `7a565550`);
- zero-fills a 14-entry × 16-byte array at `[r31+20..+244)`, then a run of
  scalar fields through `+288`;
- calls `sub_820CF958` three times, on sub-objects at `[r31+312]`,
  `[r31+336]`, and `[r31+356]` (a fourth list-shaped field at `[r31+324]` uses
  a sibling helper, `sub_820CF540`, unread this cycle).

**`0x2E3E3E38` is therefore one of these three embedded list-head fields of a
`swg::ASContext<...>` instance** — the parent object sits at `0x2E3E3E38`
minus 312, 336, or 356 (`0x2E3E3D00`, `0x2E3E3CE8`, or `0x2E3E3CD4`), none of
which match title's (`0x2E3DFA08`) or startup's (`0x2E3BFA08`) already-named
context slots. This is a third, previously-unnamed `ASContext` instance. Which
of the three offsets applies is not pinned this cycle — see Not established.

This also explains, for the first time with a mechanism rather than a
description, what `sub_820D5B90`'s link-check is actually testing:
**whether one of an `ASContext`'s own embedded lists has ever had a real item
linked into it, or still holds the empty-sentinel state `sub_820CF958`
installed at construction.** The check that traps is a "this list is still
empty" check on the interpreter's own bookkeeping structure, not a check on
some unrelated game object.

## Correcting `ddc49812`'s "boxing pool" framing

`ddc49812` read `0x2E3DF850`'s write history — box() constructing a
`String`/`VariableBase` there at tick 2428, then a recycler resetting it to a
self-pointing sentinel at tick 2451, then continuous churn afterward — as "the
same ASContext::String/VariableBase boxing pool's own head," implying one
continuous object or pool with a stable identity across that whole span.
That overstates the connection. Reading `sub_820CE750` (`ppc_recomp.4.cpp:8212+`)
— the allocator `sub_820CF958` calls for its sentinel node — shows it is a
**generic fixed-size-class free-list allocator**: clamps every request to a
minimum of 64 bytes, computes a size-class index, and pops the head off that
class's free list (`r11 = class_head; class_head = [r11+0]`, the textbook
small-object pool-allocator shape). `0x40` (64 decimal) is exactly what
box()'s own tick-2428 write of `[0x2E3DF850+0]=0x40` records — the
allocator's clamped block size, not a field of the `String` object itself
(which starts one word later, at `+4`, where the `String` vtable actually
lands).

So `0x2E3DF850` is not one persistent "pool head" object with a continuous
identity. It is a **physical slot in a shared small-object free list**,
handed out and returned repeatedly: box()'s temporary `String`/`VariableBase`
at tick 2428 (freed shortly after, ordinary temporary-value lifetime), then
independently handed back out to `sub_820CF958` for the `ASContext`'s own
list-sentinel node at tick 2451 (same size class, ordinary reuse — nothing
connects these two allocations except sharing an allocator size class), then
recycled further after that. `ddc49812`'s RTTI identification of the
tick-2428 occupant as `swg::ASContext<...>::String`/`VariableBase` stands as a
fact about *that* allocation; it does not carry forward to what occupies the
same address afterward, and should not have been described as one pool's
stable head.

**What still holds from `ddc49812`, restated more precisely**: `0x2E3E3E38+4`
was set once (tick 2451) to point at the sentinel node `sub_820CF958`
allocated that same tick, and is never updated again through the trap tick
(2571). Whether that sentinel slot was later legitimately freed back to the
allocator (ordinary churn, and the interpreter's own list machinery is
expected to re-fetch/re-validate rather than trust a stale snapshot) or
represents a genuine teardown that should have updated `0x2E3E3E38`'s own
field but didn't, is not distinguished by anything read so far — both are
consistent with the same observed trap.

## Not established

- Which of the three offsets (`+312`, `+336`, `+356`) `0x2E3E3E38` actually
  is, and therefore the parent `ASContext` instance's exact base address.
  Deciding this needs either a live capture of `sub_820CF958`'s caller-side
  `lr`/backtrace at the moment it writes this specific address, or a static
  argument-flow trace through `sub_820E1010`'s three call sites — neither
  done this cycle.
- What `sub_820CF540` (the fourth, `+324` list-style field, a different
  helper from the other three) does, and whether it's the same shape.
  Unread.
- Whether this third `ASContext` instance is ever the interpreter's active
  context for title's own dispatch, or a sibling instance (e.g. startup's,
  or one of the 26+ owner values `bfc927e1` found funneling through the
  shared interpreter slot) unrelated to title's own EndMode statement.
- Whether the sentinel node's later disappearance (if it happened) is
  ordinary allocator churn or a genuine missed-update bug in the engine's
  own list-teardown path — the two are indistinguishable from what's been
  read so far.

## Process note

`git log --oneline --reverse ddc49812..HEAD` is empty — `ddc49812` is still
`HEAD`, so this is the first available continuation, consistent with the
standing forward-check rule (`73bdeefc`).
