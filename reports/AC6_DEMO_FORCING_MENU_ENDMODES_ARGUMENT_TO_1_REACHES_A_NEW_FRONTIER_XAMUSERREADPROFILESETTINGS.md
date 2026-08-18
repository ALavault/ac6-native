# Forcing `menu_endMode`'s argument to `1` reaches a new frontier: `XamUserReadProfileSettings`

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run against
`recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`,
neutral store, **no button press, no other injection** — the only
intervention is the new argument override this cycle adds. Source change:
`recompilation/ace-combat-6-demo/src/guest_bridge/swg_native_call_trace.hpp`.

## The falsifier

`1edce620` traced `menu_endMode`'s raw integer argument as a destination
selector and named the concrete next experiment: force a **natural** call's
argument to an untested value and observe. Added
`apply_menu_endmode_arg_override`, gated by two env vars
(`AC6_DEMO_FORCE_MENU_ENDMODE_ARG`, `AC6_DEMO_FORCE_MENU_ENDMODE_AT_TICK`),
writing the boxed in-argument (`*context.r26`, the same address
`AC6_SWG_NATIVE_CALL`'s own `first_arg` already dereferences) **before**
`invoke_body_trace` executes the call — unlike every existing `FORCE_SWG_*`
override in this file, which patches an out-param after the callee returns.

Ran a natural, unmodified attract-loop with `AC6_DEMO_FORCE_MENU_ENDMODE_ARG=1`
gated to tick `4251` — title's own attract-timeout call, which naturally
passes `2` (`1edce620`).

## Result

```
tick=4251  AC6_SWG_NATIVE_CALL target=0x820EA4A8 (menu_endMode) first_arg=0x00000002   (natural)
tick=4251  AC6_MENU_ENDMODE_ARG_FORCED address=0x2E4011D4 value=1                       (override lands)
tick=4252  AC6_MODE_INNER mode=0x2E3C0100 state=0x00000002    (title's OWN state advances to 2 —
                                                                 the natural arg=2 call never does this;
                                                                 it takes a different branch entirely,
                                                                 per sub_8218AB98's arg==2||3 check)
tick=4254  AC6 runtime trap: unimplemented import xam.xex ordinal 537
           lr=0x821CB384
```

Ordinal 537, looked up against `analysis/demo/ac6-demo-import-thunks-v1.json`
(the campaign's own qualified import table):

```json
{"address": "0x82376464", "module": "xam.xex", "name": "XamUserReadProfileSettings", "ordinal": 537}
```

**Forcing the argument from `2` (the natural attract-loop value, which
cycles back to startup) to `1` (untested, previously only reasoned about
statically) causes title's state machine to genuinely advance into new
territory — reaching a real, unimplemented XDK kernel import in three
ticks.** This is not a crash or corruption: `outcome.kind` in the run's own
report is `"import"`, the exact category this campaign has resolved one at a
time throughout the plan's Phase 0/1. It is a clean, actionable frontier.

## Why this is different from every prior EndMode falsifier this session

Every earlier injection this session (`270dea0e` onward, the whole
`ASContext`/box+invoke thread) forced entry into a statement the real
dispatcher never fetches, and every one trapped on a null/missing link
inside machinery the injection itself bypassed getting to. This falsifier is
different in kind: it does not bypass any dispatch — the call at tick 4251
is the **same real, naturally-dispatched `menu_endMode` invocation** title's
own script issues every attract cycle. Only the four-byte boxed argument is
different. The chain it triggers (`sub_8218AB98` → state 2 → `sub_8218AA30`
→ `[title+112]==1` → the resource-load-and-reinit branch traced in
`1edce620`) is title's own, real, already-compiled transition logic,
executed exactly as written — the port simply doesn't yet implement one
kernel call it needs.

## Consequence for the plan

`5dc58584`'s standing priority (chase `CX360UnitManager`'s absent
constructor) is not contradicted by this — mission construction may still be
the deeper blocker for the render gate specifically — but this result gives
the campaign something more immediately actionable: **a concrete, named,
resolvable next import** on a path that is reachable *without* solving the
CX360UnitManager question first. Implementing `XamUserReadProfileSettings`
(read the XDK's own documented signature and the guest's actual call site
before writing anything, per this repo's standing discipline) is the direct
next step, and re-running this exact falsifier recipe past that point is how
to find whatever comes after it — possibly the menu construction itself, or
another import, or the CX360UnitManager path converging back in.

## Not established

- What `XamUserReadProfileSettings` needs from this call site specifically
  (its arguments, what the guest does with the result) — not read yet.
- Whether value `1` leads to an actual menu/gameplay screen once this import
  is implemented, or another wall shortly after — the run stops at the
  trap, three ticks past the forced call.
- Whether value `0` (startup's own natural value) or any other untested
  value reaches something different again — only `1` was tried this cycle.
- Whether this path ever converges with the `CX360UnitManager`/mission
  question, or is a genuinely separate route to a working frontend.

## Process note

`git log --oneline --reverse 1edce620..HEAD` is empty — `1edce620` is
`HEAD`. This is a new falsifier this report's own predecessor named, not a
repeat of prior work.
