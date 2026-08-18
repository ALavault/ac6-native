# Correcting `99044e0f`: the forced PC was consumed as garbage operands, not dispatched as opcodes — the falsifier tested nothing

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run. Entirely a re-read of
`99044e0f`'s own evidence log
(`/fastdata/lavaulta/tmp/ac6-endmode-verify-run/run.stderr.log`), caught
before building the next step on it.

## What `99044e0f` got wrong

`99044e0f` claimed the forced jump made the interpreter "genuinely fetch
and execute EndMode's own six words... exactly as `1fcc88b3`
reconstructed them," citing five consecutive PC-field writes
(`0x2DCB2028` through `0x2DCB2038`) as evidence. That reading did not
check *which function and which line* produced those five writes, only
that the addresses matched the expected sequence — and the address
sequence matching is exactly what a wrong mechanism would also produce.

**All five post-force writes carry `lr=0x82324854`, `function=sub_823246C0`,
`generated_line=18071`** — `sub_823246C0`'s own *internal operand-boxing
loop* advance line, the same line this campaign has cited since
`346255b2` as "the interpreter consuming one more literal argument."
**Not one of them is `generated_line=1047`**, `sub_82325160`'s outer
fetch/dispatch line — the line that would mean a word was read *as an
opcode* and looked up in the 104-entry table (`bfc927e1`). Compare the
run's own *pre-force* sequence at the same tick, which does show the
real pattern — outer fetches (`sub_82325160`, line 1047) interleaved
with handler entries and their own internal advances:

```
0x2DCB2134  sub_82325160  line 1047   <- outer fetch
0x2DCB2138  sub_82325050  line 854    <- opcode 0x08's own handler
0x2DCB213C  sub_82325160  line 1047   <- outer fetch
0x2DCB2140  sub_823246C0  line 18056  <- opcode 0x2E's handler, entry
0x2DCB2144  sub_823246C0  line 18071  <- opcode 0x2E's OWN internal advance
0x2DCB2148  sub_823246C0  line 18120  <- (loop continues internally)
[[[ forced write lands here, mid-flight inside sub_823246C0's own loop ]]]
0x2DCB2028  sub_823246C0  line 18071  <- still inside the SAME internal loop
0x2DCB202C  sub_823246C0  line 18071
0x2DCB2030  sub_823246C0  line 18071
0x2DCB2034  sub_823246C0  line 18071
0x2DCB2038  sub_823246C0  line 18071
0x2DCB203C  sub_823246C0  line 18120  <- internal loop's own exit check
```

The force landed *while `sub_823246C0` was already mid-loop*, reading
successive words for some **other** statement's own argument list (the
one that legitimately began around `0x2DCB2130`). The forced PC
redirected that loop's *next operand read*, not a fresh outer dispatch —
`sub_82325160` never regained control between the pre-force and
post-force sequences shown above; there is no line-1047 event anywhere
in between. EndMode's own `0x16` at `0x2024` was fetched, yes — but as
**argument N of an unrelated, already-in-progress call**, not as an
opcode dispatched through the table. "Boxes category 3" is real (a word
was read and passed to the box function), but it happened as garbage
data inside someone else's argument list, not as EndMode's own
statement being interpreted.

## Why the negative result is uninformative, not evidence

`99044e0f` concluded "boxing the category is necessary but not
sufficient" from a run where the category-3 word was never dispatched
through the mechanism (`sub_82325160`'s outer loop → table lookup →
handler call) that this whole campaign has established as how a
statement's opcodes turn into behavior. The absence of a call to
`sub_820EA4A8` in that run says nothing about whether a *properly
dispatched* EndMode statement would call it. This campaign's own
counter-evidence, sitting in the same session's earlier data: at tick
3001, the same six-word template with categories 4/5/6/0x13 (a
structurally identical shape) produced real native calls
(`GetCurrentLevel`, `GetCurrentMission`, `GetCurrentMode`, `OnVoice2D`) —
so nothing about this template shape rules out a call; the null result
in `99044e0f` reflects the broken injection point, not EndMode's own
statement semantics.

## What stands, and what's retracted

- **Retracted**: the claim that EndMode's statement was executed and
  found not to call anything. Not established, either way.
- **Stands**: the instrument's mechanics (one-shot write, no crash,
  clean `max_ticks` exit) and the methodological lesson about the first,
  non-one-shot attempt (`0x2DCB2024` sitting before the idle loop's own
  entry, defeating the "jump past the bound" reasoning) — both correctly
  described in `99044e0f`.
- Everything from `74756ffc` backward (the real dispatcher, both named
  `MovieController` owners, the `MovieMemory` layout, the enqueue chain)
  is untouched by this correction.

## The actual next step

A clean test requires injecting EndMode's offset somewhere a **natural**
dispatch cycle picks it up fresh — not stomping a live PC mid-loop. The
queue-drain path already traced (`6d61b5cd`/`e4e9b251`/`704b27b6`) gives
one: override `GetAt`'s own return (vtable slot 11, `sub_820D0FF8`,
already visible to `trace_swg_native_call` via the indirect-call
dispatch, which fires *before* the callee's own body runs) so it hands
back EndMode's offset instead of whatever `Add()` actually enqueued that
tick. The subsequent chain (`sub_82325288`→`sub_823251E0`, already fully
read) then runs unmodified: `pc = table_base + offset`, written fresh,
followed by a genuine `sub_82325160` entry — the actual mechanism, not a
mid-loop interruption of it.

## Gates

No source changed; this report is a re-read of an existing log. Native
gate JF, demo `ctest`, and both contract audits verified below before
commit.
