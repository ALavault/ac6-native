# Correcting `7d77d667`: the startup comparison found a destructor, not a linked list

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Two live runs against
`recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`
(neutral store, no injection — natural runs to `--max-ticks 3600`/`2600`),
plus reads of `ppc_recomp.4.cpp` (`sub_820D2B28`) and `ppc_recomp.5.cpp`
(`sub_820D6380`).

## What this cycle set out to check, and what it actually found

`7d77d667` proposed the natural next step: compare title's own `ASContext+292`
list (never populated by the trap tick, per `ddc49812`/`7102190f`) against
startup's analogous list, since startup's own `EndMode` call succeeds live
(`da27b1db`). The expectation was that startup's list would show something
*linked in* that title's never gets.

Bracketed startup's own interpreter slot (`0x2E3BFA08`) first: its `+12`
context field is written once, at **tick 265**, by `sub_82324188` — the same
function that installs title's own context at tick 2451 (`7d77d667`) —
to `0x2E3C3D14`. Bracketed that instance's own `+292` field
(`0x2E3C3E38`/`+3C`/`+40`) across the full run:

```
tick=40    poison
tick=265   sub_820CF958 constructs it (same shape as title's: [+4]=self,[+8]=0)
tick=266   [+8] starts churning: 0→1→2→3→2→1→0→1... (sub_820D6380/sub_820D5878/sub_820CFC58)
tick=2428  [+4] = 0   (sub_820D2B28, generated_line=18595)   ← 3 ticks after startup's own EndMode call (tick 2425)
```

Title's own `+8` field (already captured in `ddc49812`'s original bracket,
`0x2E3E3E40`) shows the **identical churn shape** — `0→1→2→3→2→1→0→1→2→...→6`,
through the same two functions (`sub_820D6380`/`sub_820D5878`), continuously
from tick 2452 through the trap tick 2571. `7d77d667`'s framing ("never had
anything linked into it") did not account for this — the field is far from
idle for title.

## Two corrections

**1. `[this+8]` is not an item/link count — it's a generic grow-by-N buffer
size, unrelated to EndMode.** Read `sub_820D6380` in full
(`ppc_recomp.5.cpp:5371-5465`): `sub_820D6380(this, amount)` checks a global
allocator high-water-mark, calls a grow/reallocate chain
(`sub_820CFDC0`→`sub_820CFD10`→`sub_820CE150`→`sub_820CF7C8`→`sub_820CF210`)
only if the arena is nearly exhausted, then **unconditionally**
`[this+8] += amount`. This is a `reserve`-and-append operation on a scratch
growable buffer — the kind of thing the interpreter uses constantly for
temporaries during ordinary bytecode execution. Its churn on title's own
context is routine VM activity, not evidence about EndMode's precondition one
way or the other.

**2. `sub_820D2B28` (the function that zeroes startup's `+4` at tick 2428) is
`ASContext`'s own destructor, not a "link established" signal.** Read in full
(`ppc_recomp.4.cpp:18498+`): it re-installs the identical `ASContext` vtable
(`0x820065A4` — same computation as `sub_820E1010`'s constructor, confirmed
by re-deriving the immediate) at its own `+0`, then walks **all four**
embedded lists in reverse construction order (`+356`, `+336`, `+324`, `+292`)
— for each: pop/release its contents (`sub_820CFC58`), free the head node
(`sub_820CE908`), then explicitly zero the head field. The write this cycle
observed (`[0x2E3C3E3C]=0` at tick 2428) is this destructor tearing down the
**first of four fields it destroys, in the same pass, on the same
teardown**, not a targeted "EndMode's precondition got satisfied" write.

**Startup's whole `ASContext` instance is destructed 3 ticks after its own
EndMode call succeeds — ordinary object lifecycle (construct at boot, use
through startup's mode, destruct once startup's mode completes), not a
positive signal about what gates a successful invoke.** Title's own instance
is simply never destructed by the trap tick, because title's own mode has not
completed — consistent with, but not newly explanatory of, title's
incompleteness.

## What this leaves standing, and what it retracts

**Retracted**: `7d77d667`'s implicit reading that startup's list becoming
populated (or title's staying empty) would show what's missing for title.
This comparison does not identify a missing precondition — it shows generic
container bookkeeping and an unrelated object-lifecycle event.

**Still standing, unaffected**: everything about *what* `0x2E3E3E38` is
(`ASContext(0x2E3E3D14)+292`, `7102190f`) and that it is title's own live
dispatch context (`7d77d667`'s core finding, the interpreter-slot `+12`
match). Also unaffected: in every run captured across this whole thread —
title's, startup's before its own destruction, and now checked here —
`[+4]` is observed only in two states: self-pointing (freshly constructed,
`sub_820CF958`'s own initialization) or explicitly zeroed (by the
destructor). **No run in this campaign has ever observed `[+4]` holding a
genuinely different, non-self, non-zero pointer** — i.e., this list has never
been seen in a state that would read as "linked to a real distinct node."
Whether that state is even reachable in this offline/no-mission demo
scenario, or whether `sub_820D5B90`'s check is watching for something that
simply never happens by design here, is unresolved — stated plainly as open,
not claimed either way.

## Not established

- What `sub_820CE908` (the destructor's own presumed `free()` call) and
  `sub_820D0A68`/`sub_820CEE28` (the other three lists' distinct teardown
  helpers) actually do — read only by call shape, not by body, this cycle.
- Whether any object anywhere in the full atlas ever links a real item into
  one of these four `ASContext` lists (as opposed to constructing it empty
  and later destructing it empty) — not observed in any run so far, but not
  exhaustively searched either.
- What, if anything, actually gates title's own missing completion — this
  correction closes the startup-comparison avenue without opening a
  replacement; the next candidate direction is not yet named.

## Process note

`git log --oneline --reverse 7d77d667..HEAD` is empty — `7d77d667` is still
`HEAD`.
