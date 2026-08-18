# Falsifier v2: a genuine, naturally-dispatched entry into EndMode's statement traps on a null read — twice, at two different points

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: three probe runs using a
new opt-in, one-shot, write-only instrument
(`AC6_DEMO_INJECT_ENDMODE_AT_TICK`/`AC6_DEMO_INJECT_ENDMODE_OFFSET`) that
overrides `MovieMemory::GetAt`'s own return value (vtable slot 11,
`sub_820D0FF8`) so `sub_82323BB8`'s natural per-tick queue-drain, for
owner A (`0x2E3EDA90`), dispatches whatever offset is requested instead
of whatever `Add()` actually enqueued that tick — a fresh
`sub_82325160` entry, not an interruption of one (correcting `99044e0f`'s
broken v1, retracted in `ab3aed60`). A diagnostic trace
(`AC6_DEMO_WATCH_SWG_GETAT_CALL`) located owner A's actual `GetAt` call
ticks first. `probe --until frontend`, no oracle, no button press needed
(target tick precedes the press).

## Locating a real injection point

`AC6_DEMO_WATCH_SWG_GETAT_CALL` (new, logs every `GetAt` call's tick/
`r3`/`r4`) shows owner A's collection (`0x2E3EDCD0`) is read only **8
times** in the whole run — ticks `2452, 2571, 3001(x2), 3033, 3051,
3061` — not every tick, even though `Add()` writes the array every tick
during the idle window (`e4e9b251`). The first injection attempt, at
tick 2995 (chosen without this data), silently never fired: `GetAt` is
simply not called that tick. Retargeted to tick `2571`, a confirmed call
point.

## Result: both candidate offsets trap on a null read

**`0xE04`** (`0x2DCB2024`, EndMode's own template start,
`1fcc88b3`/`b67e7f6f`/every prior report): injected cleanly
(`AC6_SWG_ENDMODE_INJECTED tick=2571 ... offset=0x00000E04`), and the
probe **traps** at that same tick — `outcome.kind=trap`,
`diagnostic="unmapped guest 32-bit read"`, `address=0`,
`lr=0x820DBAF0`, with `r29=0` and `r30=0` in the register dump. A clean,
immediate null-pointer read, not a hang or a silent no-op.

**`0xDF0`** (`0x2DCB2010`, advisor's alternate hypothesis — the
`0x07/0x02/0x07/0x01` words at `0x2010`-`0x201C` plausibly being
argument-push words for a *preceding* call, making `0xDF0` the real
group's entry rather than `0xE04`): also injected cleanly, and **also
traps** — same shape, different site: `address=0`, `lr=0x820D771C`
(different from the first trap), `r3=0` explicitly in the register dump.
Getting further before failing (a different `lr`) confirms `0xDF0` is a
distinct code path from `0xE04`, not the same crash reported twice — but
it still crashes.

## Reading

A synthetically-injected, properly-dispatched entry into either
candidate offset for EndMode's statement group reaches a null-pointer
read within the same tick, before anything resembling a native call or
task-list mutation could occur. This is a genuine, fail-closed result —
exactly the third branch this campaign's own falsifier plan
(`bfc927e1`'s memory notes, this session) named in advance: "trap/no-op →
the statement group needs preceding setup words, itself informative,
fail-closed."

Read against everything else this campaign has established: EndMode's
statement is real, present, unambiguously compiled into title's own
bytecode, and template-matches every other confirmed statement this
campaign has decoded — but reaching it via any path this campaign has
been able to construct (natural per-tick drain, forced PC, injected
queue offset) either never happens (natural) or crashes (forced/
injected). The most consistent reading across both trap sites is that
EndMode's statement depends on **context this synthetic injection cannot
supply** — a valid argument, object, or prior call result that a truly
legitimate caller would have set up before ever reaching this offset,
and that title's own script, in this scenario, never constructs either.
This is consistent with, though not proof of, the campaign's own
standing explanation for the demo's black frame: a mission-scoped
precondition (the `CX360UnitManager` gate, `eab92d66` and the
demo-render-chain memory's standing answer) that this offline/no-mission
run never satisfies.

## Not established

- The exact missing precondition at either trap site — `lr=0x820DBAF0`
  and `lr=0x820D771C` are both new addresses to this campaign, unread.
- Whether a *third* offset, or the statement reached via its real
  upstream caller (not a synthetic `GetAt` override), would avoid the
  trap — this report tested exactly two hypotheses, not an exhaustive
  search of the surrounding bytecode.
- Whether the trap is specific to owner A's own context (`0x2E3EDA90`)
  or would reproduce identically for the dominant owner or any other of
  the 24+ untested owners.
- Whether a mission actually being loaded (rather than this demo's
  offline/no-mission startup path) would supply whatever `lr=0x820DBAF0`/
  `0x820D771C` dereference and let EndMode's statement complete —
  consistent with, but not directly tested against, the standing
  `CX360UnitManager` explanation.

## Gates

Source changed: `swg_native_call_trace.hpp` gained two new opt-in
instruments (`AC6_DEMO_WATCH_SWG_GETAT_CALL`, diagnostic; and the
`AC6_DEMO_INJECT_ENDMODE_*` pair, write-only, one-shot), no effect unless
set. Both build trees rebuilt. Native gate JF, demo `ctest`, and both
contract audits verified below before commit.
