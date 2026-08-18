# `0x2E3E3D14` is title's own dispatch context, installed at the same tick `bfc927e1` already named

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live run against
`recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`, same
neutral store and injection recipe as every run in this thread
(`AC6_DEMO_INJECT_ENDMODE_AT_TICK=2571`, `AC6_DEMO_INJECT_ENDMODE_OFFSET=0xE0C`,
`--input-at 3000,16,...`), reproducing the identical trap.

## The question

`7102190f` pinned `0x2E3E3E38` exactly: `ASContext` instance `0x2E3E3D14`'s
first embedded list-head field (`this+292`). Left open: is `0x2E3E3D14` ever
title's own active dispatch context, or an unrelated sibling instance — per
`bfc927e1`, many owner instances funnel through one shared, recycled
interpreter slot, so having *an* `ASContext` at hand proves nothing about
*whose* it is without checking.

## The answer

Title's own interpreter slot (`0x2E3DFA08`, named by `bfc927e1`) has a
`+12` field — the `context` `sub_82324CE0` reads (`r3=[this+12]`) before
making its own vtable-slot-64 virtual call, per `1b87123e`'s earlier reading
of that function. Bracketing that slot's own header
(`AC6_DEMO_WATCH_ADDR_LO=0x2E3DFA08`, `_HI=0x2E3DFA20`) across the full run:

```
AC6_ADDR_RANGE_WRITE address=0x2E3DFA14 size=4 value=0xFEFEFEFE tick=40 ...
AC6_ADDR_RANGE_WRITE address=0x2E3DFA14 size=4 value=0x2E3E3D14 tick=2451
  thread=1 lr=0x820D2B1C function=__imp__sub_82324188 generated_line=17199
```

`0x2E3DFA14 = 0x2E3DFA08 + 12` — this is exactly the `context` field. Its
only real write, at tick 2451, installs `0x2E3E3D14` — **the exact
`ASContext` instance `7102190f` pinned as the parent of the field both
mainline EndMode traps check.** The writer, `sub_82324188`, is not new:
`bfc927e1` already named it as the function that (re)constructs title's own
recycled interpreter slot at tick 2451, in the same report that established
the slot is reused across owners rather than persistent. Same tick, same
slot, same writer, now shown to also install this specific `ASContext` as
that slot's own `context` field.

**`0x2E3E3D14` is title's own dispatch context** — not a sibling or
unrelated instance. `0x2E3E3E38` (`ASContext+292`) is a real, live field of
the exact context object title's own interpreter uses for its virtual
dispatch at the moment of the trap.

## What this closes

This resolves the last open item from `7102190f`'s "still open" list on
ownership. Combined with the full chain this thread has now built:

- `0x2E3E3E38` is `ASContext(0x2E3E3D14)+292`, the first of four embedded
  intrusive lists that instance owns (`7102190f`).
- That `ASContext` instance is installed as title's own interpreter's
  `context` field at tick 2451, by the same construction event `bfc927e1`
  already found (this report).
- Its `+4` link field was set exactly once, at that same tick 2451, to a
  freshly-allocated empty-sentinel node (`e3d7eca4`'s `sub_820CF958`
  reading) and never updated again through the trap tick.
- Both independently-dispatched mainline EndMode routes (`sub_820DBA18`'s
  never-taken `0x16` prefix; `sub_820E7638`'s real `box`+`0x4D` invoke step)
  check this exact field and find it still in that constructed, never-linked
  state (`ddc49812`).

**Read together, this is no longer "some object was never linked" — it is
"title's own dispatch context's first embedded list, constructed at the same
moment title's interpreter slot itself was (re)built, has never had anything
linked into it by the time either route into EndMode queries it."** Whatever
engine mechanism is supposed to populate this list (register a handler,
attach a listener, queue a pending completion — its purpose is still
unnamed) never runs for title, in this offline/no-mission demo scenario, on
either currently-known dispatch path.

## Not established

- What this specific list (`ASContext+292`) is *for* in engine terms —
  `sub_820D5B90`'s check is precisely scoped now (this list, this instance)
  but its purpose (what would normally link something into it, and what that
  would mean for EndMode's completion) is still unnamed.
- Whether any *other* code path in the full atlas — not just the two
  mainline routes already found — ever links something into this list for
  title's own context, at any tick.
- Whether the same is true for startup's own analogous list at the
  equivalent offset of *its* context instance (startup's `EndMode` call is
  known to succeed live, per `da27b1db`) — checking that would be the
  natural falsifying comparison: if startup's own `+292` list is non-empty
  by the time its `box`+`0x4D` invoke fires, that directly names what's
  missing for title.

## Process note

`git log --oneline --reverse 7102190f..HEAD` is empty — `7102190f` is still
`HEAD`.
