# Correcting `69ff833a`: startup's own `CSwgCallback+9` never pulses either

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `AC6_DEMO_WATCH_ADDR_LO`/`HI`
watching startup's own `anim` (`CSwgCallback`) instance
(`0x2E3BF0D4+8..12`, address confirmed live via the same run's `AC6_SWGW`
trace at tick 266), `probe --until frontend --max-ticks 2500`, neutral
route — spanning startup's entire observed active window (state 1 begins
tick 266, state 2 confirmed by tick 2426, per `AC6_MODE_STATE`). No oracle.

## What this corrects

`69ff833a` reasoned: since startup's state 1→2 transition is real and
`sub_820CE368` (shared by both classes) provably never writes state
itself, the write "most plausibly" happens inside the `CSwgCallback+9`-
gated block, and inferred "startup's own `CSwgCallback` instance evidently
does get pulsed at some point" — stated as an inference, not measured.
Measuring it: **exactly four writes to startup's own flag byte across the
whole 2500-tick window — the same construction-only pattern already found
for title (`bdb437e6`): poison, zero-init, `[+8]=1`, `[+9]=0`. Never
touched again.** The flag-gated block never ran for startup either, and
startup's state still advanced. **The inference was wrong; retracted.**

## Where the signal has to be instead

`sub_820CE368` makes exactly two calls *before* the flag check that run
unconditionally, every invocation, for both classes:

1. `[CNuTimer.vtable+28](CNuTimer_instance)`, called **twice**. The
   instance (`0x823CA848`) RTTI-resolves to `.?AVCNuTimer@ACE6@@` — a
   single fixed global timer object, not per-`CSwgManager`. Almost
   certainly a clock/delta-time tick, not a per-task completion signal;
   deprioritized as a candidate since it can't differentiate startup from
   title (same singleton, same call, same arguments, every tick, for
   both).
2. `[ [[this+24]+0] +116 ](r3=[this+24])` — a call through a **different,
   per-instance** sub-object at `[CSwgManager+24]` (not `field4`/`world`,
   a field not previously examined). This one *can* differ between the two
   classes' `CSwgManager` instances and was not identified or read.

Given the flag-gated block is now ruled out for both observed cases (empty
for title always, empty for startup during its actual transition), **this
`[this+24]` sub-object's `vtable+116` call is now the leading candidate**
for where a real per-task "are you done" check (and possible state write,
transitively) lives.

## Not established

- What `[CSwgManager+24]` is, for either instance — not read.
- Whether `[[this+24]+0]+116`'s call, transitively, ever writes
  `[title_this+12]` or `[startup_this+12]` — not traced.
- Whether `CNuTimer`'s tick has any indirect bearing (e.g. gating some
  *other* reachable code via elapsed time) — deprioritized, not ruled out.

## Gates

No source changed; report-only commit.
