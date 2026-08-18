# Startup's own successful `EndMode` call, decoded live for the first time, reveals a box-then-invoke mechanism in a statement shape different from title's

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: three probe runs
(`probe --until frontend --max-ticks 3600`, correctly-timed START,
headless, no oracle) bracketing startup's own execution-context slot
(`0x2E3BFA08`, established distinct from title's `0x2E3DFA08` in
`346255b2`) and its bytecode buffer (`table_base=0x2DF010B0`, read live
from `[0x2E3BFA08+16]`), plus `AC6_DEMO_WATCH_SWG_BOX_CALL`/
`AC6_DEMO_WATCH_SWG_NATIVE_CALL` together at the exact tick startup calls
`EndMode` (tick 2425, `642f77a4`/`7a565550`). Static: `sub_82324CE0`'s
full generated body (`ppc_recomp.44.cpp:324-368`), and the 104-entry table
dump (`bfc927e1`) for opcode `0x4D`.

Two small fold-in corrections first, per house style (no dedicated
commits): `3b12d584`'s claim of "covering both gaps in one experiment"
overreaches — a null combined-force result doesn't strictly subsume the
individual `GetCurrentMode`/`GetCurrentLevel` tests, since a masking
interaction (unlikely, not ruled out) could in principle hide an effect
either alone would have had; doesn't change that thread's exhausted
status. And an early reconstruction attempt in this report's own working
notes byte-split a 4-byte value little-endian (`value >> 8*i`) against a
big-endian PPC guest — caught before it produced a wrong conclusion by
the known-plaintext check below, not shipped as a finding.

## What this closes

This campaign has run its whole EndMode falsifier arc (`1fcc88b3` through
this session's `270dea0e`/`b29dcf77`/`d6fb7982`/`73bdeefc`) entirely
against **title's own, never-successfully-executed** statement at
`0x2DCB2024`. No genuinely successful, naturally-occurring EndMode
invocation had ever been decoded live in this campaign — every prior
measurement of "what EndMode's call looks like" was either static
reconstruction of an unreached statement, or a forced/injected dispatch
that trapped. Startup's own call (tick 2425, `target=0x820EA4A8`,
`642f77a4`) is a genuine positive control that has simply never been
decoded before. This report decodes it.

## Reconstructing the buffer correctly — a real trap avoided

Bracketing writes to `[table_base, table_base+0x600)` across the *whole*
run and taking last-write-wins per byte reconstructs floating-point-
looking garbage (`0x3f800000`, `0x43701...`, etc.) — not bytecode. The
first instinct, to write this up as "startup's buffer isn't bytecode
either" or as an inconclusive result, would have been exactly the kind of
under-checked negative this campaign's own discipline exists to catch.
Grepping the single address the live trace *proves* must hold an opcode
at tick 2425 (`0x2DF01518`) shows why: the loader (`sub_82278F78`,
confirmed by name) writes it once at tick 257, then **writes it again at
tick 2439** — 14 ticks *after* the tick-2425 dispatch this report cares
about. The heap slot is reused once startup's own script tenure ends
(startup's teardown sits right at this boundary, consistent with this
campaign's own documented heap-reuse-at-teardown pattern for the
`MovieMemory` arrays, `74756ffc`). Filtering to `tick <= 2425` and
assembling bytes big-endian (not little-endian — the bug in this report's
own first attempt) gives full, gap-free coverage of the window and
resolves cleanly.

**Two independent known-plaintext checks pass.** The live trace at tick
2425 shows PC entering `sub_823246C0` (opcode `0x2E`, `bfc927e1`) at
`0x2DF01518`, and later entering `sub_82324CE0` at `0x2DF0154C` (PC
recorded as `0x2DF01550` — one fetch-advance past the actual opcode word,
same convention as every other dispatch site this campaign has read).
The tick-filtered reconstruction gives `word@0x2DF01518 = 0x2E` and
`word@0x2DF0154C = 0x4D` — and the 104-entry table (`bfc927e1`'s own
static dump, re-checked here) confirms index `0x4D` (77) resolves to
`sub_82324CE0` exactly. Both checks pass; the reconstruction is trusted
from here.

## The decoded sequence, and what actually triggers the native call

Reconstructed words from the dispatched entry (`0x2DF01518`) onward:

```
0x2E 0x18 0x07 0x00 0x07 0x01 0x00 0x06 0x19 0x2E 0x08 0x00 0x01 0x4D 0x17 0x00 0x2E 0x18 ...
```

**This does not match `1fcc88b3`'s `[0x16, 0x19, 0x2E, 0x08, 0x00,
category]` template at all — it starts directly with `0x2E`, never `0x16`
anywhere in this window.** Cross-referenced live against
`AC6_DEMO_WATCH_SWG_BOX_CALL` at the same tick:

```
box(r4=6)  pc_after=0x2DF01538   <- sub_823246C0's first invocation, boxes value 6
box(r4=1)  pc_after=0x2DF0154C   <- second invocation, boxes value 1 == startup's own EndMode category (7a565550)
[outer dispatch fetches word@0x154C=0x4D, calls sub_82324CE0]
AC6_SWG_NATIVE_CALL target=0x820EA4A8   <- fires HERE, before sub_82324CE0's own trace line
[sub_82324CE0's own PC-advance write, its last instruction, appears after]
```

`sub_82324CE0`'s generated body (read in full): `r3 = [this+12]` (the
context/container, same field `AC6_SWG_BOX_CALL`'s own `r3` reads),
`r4 = [this+20] - [this+16]` (current offset from table_base — the same
`r4` box's underlying call, `sub_820DA488`, receives, but here computed
directly rather than passed as a literal), then `r11 = [[r3+0]+256]`
(**the context object's own vtable, slot 64** — `256/4` — a *different*
slot from box's slot 20) and `bctrl`s it. Only *after* that virtual call
returns does it do `[this+20] = [this+16] + r3` (table_base plus the
call's own return value — a data-driven PC advance, not a fixed step).

**The native-call trace line sits between the dispatch-into-`sub_82324CE0`
event and `sub_82324CE0`'s own final PC-advance write** — the only place
in its four-instruction body where a nested call happens is the `bctrl`
at slot 64. This places the native call to `sub_820EA4A8` **inside that
virtual call**, not inside `box()`/`sub_823246C0` itself. **Opcode `0x4D`
(`sub_82324CE0`) is the actual invoke step; `box()` (opcode `0x2E`)
prepares/resolves arguments and the category (symbol) reference that
`0x4D`'s virtual call then acts on.**

## What this does and doesn't say about title's own statement

**Does not retract or weaken anything this session established about
title's own statement at `0x2DCB2024`** — that statement is real,
present, and traps on a null read inside `sub_820D5B90` when forcibly
dispatched (`9bd4e0b9`/`270dea0e`), independent of whatever startup's own
shape looks like.

**Does open a genuinely new question**: title's statement is prefixed
`[0x16, 0x19, 0x2E, ...]`, startup's successful one is `[0x2E, 0x18,
...]` — two different leading opcodes. `0x16` dispatches (per this
session's own falsifier trace) to `sub_820DBA18`, which calls the shared
link-check helper `sub_820D5B90` before (presumably) doing something
else — its body past that call is unread. The natural, well-motivated
reading, consistent with everything established so far: `0x16` is a
*validated* invoke form (link-check first, then presumably proceeding to
something functionally equivalent to `box`+`0x4D`), while `0x2E`-first is
a *direct* form used when no such validation is needed — not two
unrelated mechanisms, but the same underlying invoke step reached via two
different front doors. This is a hypothesis, not yet checked.

## Not established

- `sub_820DBA18`'s own body past its call to `sub_820D5B90` — unread.
  This is the concrete next static step: if it proceeds to a `box`/`0x4D`-
  shaped sequence after a successful link-check, that would directly
  connect the "validated" and "direct" forms as one mechanism with an
  extra guard, rather than two unrelated statement kinds.
- What vtable slot 64 resolves to on startup's own context class, and
  whether it's the same slot (same class) title's context would use —
  not read.
- Whether `box(6)`'s own value (`6`) means anything specific, or is an
  unrelated preceding statement's own argument, not part of the
  `EndMode` call itself — not determined; only `box(1)` was confirmed to
  match the category value.
- Whether title's script, if `sub_820D5B90`'s link-check *did* pass for
  its own object, would proceed through an equivalent `sub_82324CE0`-style
  invoke and successfully call `sub_820EA4A8` — not tested; would require
  either fixing the missing link (unknown how) or a further forced
  injection past that specific check.

## Gates

No source changed; report-only commit, entirely live/static re-analysis
using existing instruments (`AC6_DEMO_WATCH_ADDR_LO/HI`,
`AC6_DEMO_WATCH_SWG_BOX_CALL`, `AC6_DEMO_WATCH_SWG_NATIVE_CALL`, all
pre-existing). Native gate JF, demo `ctest`, and both contract audits
verified below before commit.
