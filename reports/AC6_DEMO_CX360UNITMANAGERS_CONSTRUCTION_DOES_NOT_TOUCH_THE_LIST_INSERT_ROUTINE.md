# `CX360UnitManager`'s construction does not touch the list-insert
# routine — two separate unreached facts, not one unified cause

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Static read only: all seven `CX360UnitManager`
construction-root functions `f03eed93` named
(`0x82093840`/`0x82095958`/`0x82099F20`/`0x82174888`/`0x82176930`/
`0x8217C0E8`/`0x8217C258`, `ppc_recomp.0.cpp`/`ppc_recomp.14.cpp`), read in
full and grepped for the list-insert routine and offsets `02949a71` named.
No probe run, no source change.

## What this checks, and why

`02949a71` sharpened `sub_820D5B90` (the helper `EndMode`'s invoke step
traps on) to a generic "intrusive list is non-empty" assertion, and named
the open question as "what code is ever supposed to insert into this
list." The standing, much older hypothesis in this codebase's history is
that `CX360UnitManager` (unreached on every route, `f03eed93`: 7/7
construction roots, all executed by the Xenia oracle) is the missing
piece behind multiple stalls this campaign has found. This report checks
that hypothesis directly, at the address level, rather than continuing to
carry it forward unverified — per this repo's own standing rule, "a
plausible rule with no control is refused."

**Correction to a wrong lead first**: a search for `CX360UnitManager`
initially surfaced `reports/cycle-1385-a-named-class-at-last.md` and
neighboring numbered-cycle reports as promising. They are not usable here
— that whole numbered-cycle lineage investigates
`reconstruction/ace-combat-6` (the retail-game reconstruction, different
gate, different `ctest` count, addresses like `0x8222BEC8`), not
`recompilation/ace-combat-6-demo`. `CX360UnitManager` is a namesake class
in a structurally different binary; nothing in that lineage transfers to
this XEX's own addresses.

## The actual check

Read all seven roots `f03eed93` (`reports/AC6_DEMO_RENDER_GATE_RAISER.md`)
named, in full (60-280 generated lines each). Every one of the seven
constructs the `CX360UnitManager` subobject at a fixed displacement,
`+3248`, inside a larger owning object — the one literal that recurs
across all seven, confirming `f03eed93`'s own finding, nothing new.
Grepped each body for a call to `sub_820D5BE0` (the list-insert routine)
or `sub_820D5B90` (the emptiness-check helper), and for any store
instruction targeting offset `292`/`324`/`336`/`356` (decimal) or their
hex equivalents (`0x124`/`0x144`/`0x150`/`0x164`), the four `ASContext`
list-head fields this thread has been tracing.

**Zero matches, in all seven functions.** No call into the list-insert
routine, no store to any of the four traced offsets, direct or as an
immediate operand.

## Conclusion

**The two threads do not connect, at the level this check can see.**
`CX360UnitManager`'s construction path and title's `ASContext` list-insert
question are two independently-unreached mechanisms with no address-level
overlap. The hypothesis that they share one root cause — plausible on its
face (both are "never happens in this offline demo," both plausibly
mission-scoped) — does not survive a direct code check. This does not
prove them unrelated through some deeper call chain (the seven roots'
*own* bodies were read, not their full downstream call graphs, which this
report does not trace), nor rule out a shared upstream trigger neither
list literally calls the other for. But no evidence for a connection was
found where the check was cheapest to run, and carrying the hypothesis
forward without this check would have been exactly the "plausible rule
with no control" this repo's own discipline refuses.

**Honest state of the render-chain investigation after this check**:
there remain two separate, well-evidenced, still-unexplained "never gets
constructed / never gets populated" facts — `CX360UnitManager`'s absence
(render-submission-gate side, already fully traced to
`sub_821C57D0`/`device+0x5460`) and the `ASContext` list's absence
(`EndMode`-invoke-side, this session's thread) — not one unified cause
wearing two faces. Both may still turn out to share a single upstream
reason (e.g., both gated on the same "no mission is loaded" runtime
decision, made somewhere neither thread has reached yet), but that is now
an open hypothesis stated as such, not an assumption this campaign should
keep building on unchecked.

## Not established

- Whether a deeper call-graph trace from any of the seven roots (not just
  their own bodies) eventually reaches the list-insert routine — not
  traced.
- Whether some *other*, not-yet-identified construction path (not one of
  `f03eed93`'s seven roots) is responsible for populating the `ASContext`
  lists — not searched for.
- What, if anything, is the actual shared upstream trigger (if one
  exists) that would explain both absences at once — not identified by
  this report; this report only rules out the one specific direct
  connection it checked.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change — pure static read.
