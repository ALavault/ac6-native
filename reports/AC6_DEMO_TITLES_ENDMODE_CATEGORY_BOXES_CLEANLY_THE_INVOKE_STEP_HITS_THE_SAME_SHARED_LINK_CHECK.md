# Title's `EndMode` category boxes cleanly — the invoke step itself hits the same shared link-check, at a third site

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: one probe run,
`AC6_DEMO_INJECT_ENDMODE_OFFSET=0xE0C` (targeting `0x2DCB202C`, the `0x2E`
word directly — skipping the `0x16` prefix op this report's own
predecessor identified), same tick 2571 / same collection default as
every prior injection, `AC6_DEMO_WATCH_SWG_BOX_CALL`/
`AC6_DEMO_WATCH_SWG_NATIVE_CALL`/PC-bracket all enabled together,
`probe --until frontend`. Static: `sub_820E7638`'s generated body
(`ppc_recomp.6.cpp:16174-16460+`).

## Two corrections this report requires, both from the same root cause

**`AC6_DEMO_STARTUPS_SUCCESSFUL_ENDMODE_CALL...md`'s (`da27b1db`) own
"validated wrapper" hypothesis is wrong.** It proposed `sub_820DBA18`
(the `0x16`-prefix op's target) might, after its link-check, "proceed to
an equivalent `box`+`0x4D` sequence." It does not — `sub_820DBA18`'s own
body (read in full for the first time in `da27b1db`, re-checked here) is
a long, unrelated construction/formatting routine with no `box`- or
`sub_82324CE0`-shaped call anywhere in it. The `0x16` prefix and the
`box`+`0x4D` invoke pair are two **separate, independently-dispatched**
statement fragments in the buffer, not one wrapping the other — this
report's own injection, entering directly at the `0x2E` word and skipping
`0x16` entirely, proves the pair is independently reachable.

**`b29dcf77`'s "EndMode is ruled out as the missing command" needs
sharpening, not reversal.** The practical outcome it reported — title
never successfully calls `sub_820EA4A8` — still stands, confirmed again
by this report. What was imprecise: `b29dcf77` and every report before it
treated "EndMode's statement" as the single six-word unit at
`0x2DCB2024-0x2DCB2038`, and the earlier falsifier runs (`9bd4e0b9`)
tested offsets `0xE04`/`0xDF0`, both of which land on the `0x16` prefix
or mid-argument-data for it, never on the `box`+`0x4D` pair itself. This
report is the first to actually dispatch that pair.

## The result: `box(3)` fires cleanly; the invoke traps, at a third distinct site

```
0x2DCB202C (injected entry)          <- word=0x2E
0x2DCB2030 (line=1047, outer fetch)
0x2DCB2034 (sub_823246C0 entry, line 18056)
0x2DCB2038 (line 18071)
0x2DCB203C (line 18120, exit)
AC6_SWG_BOX_CALL tick=2571 r3=0x2E3E3D14 r4=0x00000003 r31=0x2E3DFA08 pc_after=0x2DCB203C
0x2DCB2040 (line=1047, outer fetch of word@203C -- table[0x4D])
[[[ trap: unmapped 32-bit read, address=0, lr=0x820E7840 ]]]
```

**`box(0x00000003)` genuinely fires** — a real `AC6_SWG_BOX_CALL` event,
category `3`, on title's own context (`r31=0x2E3DFA08`, `r3=0x2E3E3D14`,
the context field `346255b2` already established), consuming exactly two
words (`[0x08, 0x00, 0x03]`, matching startup's own working shape
`[0x08, 0x00, 0x01]` word for word except the category). **This confirms
`7a565550`'s symbol-table finding on live data for the first time**:
title's script genuinely can resolve and box category 3 through the real
mechanism — the binding is not just present in a static table, it works
end to end through `box()`.

**Then the outer dispatcher fetches word `0x4D` at `0x203C` and calls
`sub_82324CE0` — same opcode, same shape as startup's own successful
invoke — and traps.** `lr=0x820E7840` sits inside a *third*, previously
unread function, `sub_820E7638`, at its own call into **the same shared
helper this campaign has now found three times**, `sub_820D5B90`
(`270dea0e`/`73bdeefc`): `sub_820E7638` reads a self-referential link
field off an object in `r27`, exactly the same `[obj+4]` /
`[[obj+4]+4]`-compare shape as `sub_820DBA18` and `sub_820D7700` before
it, and calls `sub_820D5B90(r27)`. Not traced here whether `sub_82324CE0`
calls `sub_820E7638` directly (as its own vtable-slot-64 target for
title's context class) or through one more level of indirection — the
call chain from `sub_82324CE0`'s own `bctrl` down to this exact `lr` is
inferred from tick/PC coincidence, not stepped instruction by
instruction.

## Reading

**The precondition failure is not specific to one opcode's own
preamble.** Three independent call sites — `sub_820DBA18` (the `0x16`
prefix op, `9bd4e0b9`), `sub_820D7700` (the `0xDF0` offset, now understood
per this report to have landed on box-argument data rather than an
alternate statement entry, `270dea0e`), and now `sub_820E7638` (the
invoke step itself, reached via the *correct*, validated `box`+`0x4D`
path) — all reach the identical shared link-check helper
(`sub_820D5B90`) and all fail it, on different objects each time. This is
a materially stronger form of the campaign's standing "missing setup"
reading than anything established before: it is not one broken object
blocking one code path, but the same class of precondition — an object
never linked into whatever runtime structure `sub_820D5B90` validates —
recurring across every distinct route this campaign has found into
title's `EndMode` machinery, including the one route (`box`+`0x4D`) that
demonstrably works for startup.

**Net effect on the campaign's conclusion**: unchanged in outcome
(title never calls `sub_820EA4A8`, `b29dcf77`'s headline stands), but the
*reason* is now resolved much more precisely — not "the statement can't
be reached or dispatched" (it can: `box(3)` proves it), but "the object
graph the invoke step depends on is never linked for title's own context,
by the same mechanism that blocks every other path this campaign has
tried." Consistent with, and now considerably better evidenced than
before for, the mission-scoped `CX360UnitManager` explanation.

## Not established

- What `r27` (this trap's own failing object) is, and whether it is the
  same object family as `sub_820DBA18`'s `r31`/`sub_820D7700`'s `r31+312`
  — not identified; all three are heap addresses in the `0x2E3Exxxxx`
  range typical of this campaign's objects, not cross-checked further.
- The exact call path from `sub_82324CE0`'s own `bctrl` (vtable slot 64)
  to `sub_820E7638` — inferred from tick/PC coincidence in this run, not
  read instruction-by-instruction.
- Whether any object in this whole family is EVER properly linked
  anywhere in this run (for any owner, any tick) — not swept; every
  report to date has only ever found it unlinked.
- What `sub_820D5B90` is FOR — still read only as "a self-referential
  link-field validator," never named, never connected to a specific
  engine subsystem (list membership, reference count, registration
  table) by anything stronger than its own instruction shape.

## Gates

No source changed. Native gate JF, demo `ctest`, and both contract
audits verified below before commit.
