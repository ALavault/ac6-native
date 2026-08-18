# EndMode is ruled out as title's missing completion command — the state-advance chain and the falsifier now connect

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run. This report connects two
threads of the same campaign that ran in parallel across many commits and
had not yet been read against each other in one place: the state-advance
chain (`642f77a4`, established before this session) and this session's
EndMode falsifier work (`1fcc88b3`→`b67e7f6f`→...→`9bd4e0b9`→`270dea0e`).
One correction folded in below, per house style (no dedicated commit).

## The established state-advance mechanism

`642f77a4` traced, hop by hop, how the demo's one confirmed state
transition (startup → title, tick 2425/2429) actually fires:

```
swg script reaches a scripted cue
  -> sub_820E8F90 (native-call marshaller; reached 9x, ticks 1045-3001)
     -> sub_820EA4A8 (listener-array pump, calls slot +0x54 on each entry)
        -> sub_821728C0 (startup's own listener's +0x54 handler)
           -> unconditionally: startup_this+12 = 2
              -> state-2 countdown -> mode manager switches state
```

Title's own listener has the structurally identical `+0x54` slot
(`0x8217C890`) available, and it is **directly confirmed unreached** in the
full 12000-tick atlas — no `indirect_edges` entry ever targets it.
`642f77a4`'s own conclusion: the open question is which native command, if
any, title's own script is scripted to invoke that would reach this chain,
and whether it ever issues it — "a script-content/script-decision
question," reopening whether `GetCurrentMode`/`GetCurrentMission`/
`GetCurrentLevel` returning constant fallback values steers the script away
from ever issuing it.

## EndMode was this campaign's standing candidate for that missing command

Independently, `1fcc88b3`/`b67e7f6f` and the whole thread since established
that EndMode (symbol-table category 3, `7a565550`/`33b549ef`) is compiled
into title's own bytecode, present and address-checked, yet **never fetched
by the interpreter's outer dispatch in any observed run** — the same shape
as "title's script never issues [the missing command]." EndMode was the
natural candidate: a real, present statement that the script structurally
avoids, matching `642f77a4`'s own open question almost exactly.

## What this session's falsifier settles: EndMode cannot be that command, reachability aside

`270dea0e` (this session, building on `9bd4e0b9`) forced a genuine,
naturally-dispatched entry into EndMode's statement via a different route —
overriding `MovieMemory::GetAt`'s return so the real per-tick queue-drain
picks up EndMode's own offset instead of whatever title actually enqueued.
Verified genuine (real `sub_82325160` line-1047 fetches present in both
runs, not a mid-loop PC stomp). **The result: EndMode's statement traps on
a null read at its very first word**, inside a shared helper
(`sub_820D5B90`) that validates whether an object is linked into some
runtime structure, before the statement's own words ever reach anything
that could call `sub_820E8F90`'s marshaller, `sub_820EA4A8`'s pump, or
title's own `+0x54` handler.

**This means EndMode is ruled out as the state-advance chain's missing
command independent of the reachability question `642f77a4` left open.**
It is not merely "never issued" (a script-decision question, as
`642f77a4` framed it) — even with reachability artificially granted by
force, it cannot structurally get far enough to issue anything. Whatever
title's script is supposed to invoke to reach `sub_820EA4A8` and unstick
its own `+0x54` handler, it is not EndMode's statement.

## One correction, folded in

`270dea0e`'s own text describes `sub_823246C0` (opcode `0x2E`'s handler) as
"the one `0xE04` eventually reaches downstream," implying it's on the path
the `0xE04` offset takes before its trap. It is not, in the evidence that
report itself presents: run2's post-injection PC-bracket log shows exactly
one `sub_82325160` line-1047 fetch (the outer dispatch consuming word
`0x16` at `0x2024`) and then the trap at `lr=0x820DBAF0` — `sub_823246C0`
never appears anywhere in that specific run's log. The claim was carried
over from earlier reports' template-matching (EndMode's word `0x2E` at
offset `+8` names `sub_823246C0` as *a* word in the template) rather than
from this run's own trace. Harmless to `270dea0e`'s conclusion (both
offsets still trap on a null read, inside the same shared helper either
way) but worth naming precisely rather than left implied.

## What this reopens, and what it doesn't

Does not reopen: the mechanism work standing since `74756ffc` backward
(dispatcher, `MovieController`/`MovieMemory` layout, enqueue chain,
cross-validation) — untouched by this connection.

Reopens, as the concrete next step: `642f77a4`'s own still-open item —
instrument `sub_820E8F90`'s 9 calls (ticks 1045-3001) for their per-call
command tag and arguments, and check whether any of them is a
query/branch (`GetCurrentMode`/`GetCurrentMission`/`GetCurrentLevel`,
`3c7e7291`) whose constant fallback answer steers title's script away from
ever reaching `sub_820EA4A8` — rather than continuing to look at EndMode's
own statement, now closed. This is the direct, named next unit of work for
the campaign's primary frontend/state-advance thread, as distinct from the
independent Phase 0/1 kernel-import-diff track the project plan also
names.

## Not established

- What `sub_820E8F90` actually marshals at each of its 9 calls — the
  concrete next instrument, not built here.
- Whether any of those 9 calls is itself the missing command under a
  different name, already issued but not reaching `sub_820EA4A8` for a
  different reason (e.g., a wrong argument, not a wrong choice of command).
- Whether `0x8217C890` (title's own `+0x54` handler) behaves identically to
  `sub_821728C0` if ever reached — still unread, moot until something calls
  it.

## Gates

No source changed. Native gate JF, demo `ctest`, and both contract audits
verified below before commit.
