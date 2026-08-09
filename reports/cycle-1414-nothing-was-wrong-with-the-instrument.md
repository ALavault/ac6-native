# Cycle 1414 — nothing was wrong with the instrument

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.** No new Ghidra run — this cycle re-used
  cycle 1413's two calibration batches, which is the point of having both.
- No product C++ changed; ctest stays **51**. **No contract entry.**
- tools/tests 77 → **79**. `CLAUDE.md` gains a rule.

## The answer, and it is the cheap one

Cycle 1413 closed on an open finding: the harness calibration reported **0 of
138** where `CLAUDE.md` and the plan both cite 138/138, and reverting the
harness to HEAD reproduced the same 0, so it predated that cycle.

**Nothing was wrong with the harness.** The comparator was.

The general harness emits `region_dumps` **always** — as `[]` when no spec asked
for a dump. The specialised `MicroExecuteScenarioParser.java` that produced the
138 committed snapshots had no `dump` directive and no such key. `normalise`
dropped `schema`, `identity`, `provenance` and `notes`, and kept this one. So
every one of the 138 cases differed by exactly `{"region_dumps": []}` against a
key that was absent.

That is the same shape cycle 1321 already handled once, for `after_hex_b`: **a
field the old harness could not produce is not a semantic difference.** The
lesson was written down and the next field to arrive was not covered by it.

## For 87 commits

| | |
|---|---|
| `region_dumps` arrived with the `dump` directive | `f980b88a`, 87 commits ago |
| the calibration was last touched | `a829f4eb`, 93 commits ago |

So it broke the moment `dump` was added and has been reporting 0 of 138 ever
since, unread, while two documents cited the passing figure. It is not run by any
gate — it needs a Ghidra pass — which is precisely why it rotted.

## The fix, and why it is conditional

```python
CONDITIONALLY_IGNORED_WHEN_EMPTY = ("region_dumps",)
```

Dropped **only when empty**. An unconditional exclusion would have been one line
shorter and would have made the calibration blind to a spec that gained a real
dump — which is a change worth failing on. Two unit tests pin both halves:
an empty `region_dumps` compares equal, a non-empty one does not.

## And both harness versions pass

```
committed harness (cycle 1413's control batch)   cases=138 equal=138 failures=0
with cycle 1413's fctid and fmadd overrides      cases=138 equal=138 failures=0
```

The second line is the one that matters beyond this cycle: it is the calibration
cycle 1413 owed and could not read. **Its two overrides change nothing the 138
cases measure**, which is what an override registered behind a spec directive
should do and is now demonstrated rather than assumed.

## What actually failed here

Not the harness, and not really the comparator either. The calibration is the
only check on the instrument that every other measurement rests on, and **it runs
only when someone remembers**. Ninety-three commits is how long that lasted.

So the rule goes where it will be read — `CLAUDE.md`, beside the gates:

> **When you change `scripts/MicroExecuteFunction.java`, re-run the calibration
> in the same cycle.**

It cannot be a gate: gates run every commit and this needs a Ghidra pass. Tying
it to the one event that can invalidate it is the closest cheap thing.

## A note on how it was found

Cycle 1413 did not go looking. It changed the harness, ran the calibration
because changing an instrument obliges you to re-measure it, and got 0 of 138 —
then had to prove its own change innocent, which is what dated the regression.

The discipline that says *measure the instrument before trusting it* paid twice
in two cycles: once by finding two real defects, once by finding a false alarm
that had been sitting under a passing headline for three months of commits.

## Not established

- Whether any other tool compares whole snapshot documents and would break the
  same way on the next field the harness gains. `compare_ac6_function_snapshots.py`
  is the obvious candidate and was not audited here.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
audit_microexec_harness_calibration   cases=138 equal=138 failures=0
audit_vmx128_behaviours               pass
```

## Next

The instrument is trustworthy again and the schedule estimate holds: this was the
half-day reading, not the drift. Back to the demo.

**Wire the position into the session.** `integrate_flight_position` (`0x82303110`)
is ported, tested and contracted, and `FlightSessionState` carries no position at
all — so "the aircraft does not move" is a wiring gap before it is anything else.
The one thing to read rather than assume first: what the integrator's `rates`
mean physically, world axes or body axes, because a demo that flies sideways
would be a wiring choice presented as retail's arithmetic.
