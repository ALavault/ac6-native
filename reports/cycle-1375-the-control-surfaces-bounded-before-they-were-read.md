# Cycle 1375 — the control surfaces, bounded before they were read

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_controls.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 35**, was 34.
- New tool `tools/audit_flight_controls_microexec.py`, new artefact
  `analysis/flight/flight-controls-microexec.tsv`.
- **Contract: the sixteenth behaviour**, `retail_flight_controls`.

## Measure before reading

`0x82302DB0` is slot 30 of `0x8200F270` — the first of the three pure virtuals
the step calls. 216 instructions is enough to read badly.

So it was **bounded first**. Resolving every `lfs` gave fourteen constants in one
pass. Counting mnemonics gave **zero calls and zero vector instructions**. And a
capsule seeded the model with a per-offset pattern, ran the function, and dumped
it back: **eight words changed**, and they are exactly the eight `stfs` offsets
in the listing. Static and dynamic footprints agreeing is the check; either alone
would have been a claim.

That turned "read 216 instructions" into "read fourteen fields", and the reading
took one pass.

## What it is

A control-surface rate limiter, in three parts.

**Two primary ramps** at `+360`/`+364`, gated by `[+48]`/`[+52]`: they rise at
**10/3 per second** while held and fall at **half that** when not, clamped to
[0, 1]. Full travel in 0.3 s, full decay in 0.6 s.

**Two secondary ramps** at `+368`/`+372`, at **10.0 per second** up and 0.8 of
that down, which only rise once their primary passes **0.99** — and which are
**assigned zero**, not left to decay, when the gate is shut. `+376` is
`[+360] − [+364]`, a differential of the pair.

**Three axes** at `+304`/`+308`/`+312`: each self-centres by `0.7 × rate` without
crossing zero, then takes its command. They are not interchangeable:

| field | rate | command | limits |
|---|---|---|---|
| `+304` | 5/3 | scaled, gain **1.0 up and 0.9 down** | **[−0.9, +1.0]** |
| `+312` | 5/3 | scaled by the rate | [−1, +1] |
| `+308` | 2.5 | **its SIGN only** — the magnitude is never used | [−1, +1] |

When bit 2 of `[+332]` is set, all three rates are divided by
`([+344] + 0.1) × 10` — an authority reduction keyed on the **same field that
scales gravity** in the integrator.

## Two readings that go the other way from the guess

**The interlock.** `0x82302EF8` compares `[+360]` against zero and `beq`s **past**
both stores, then does the same for `[+364]`. So the zeroing of `+368` and `+372`
runs only when **neither** primary is zero — both live kills both secondaries.
Reading `beq` as "do" rather than "skip" inverts it, and the control
`CONTROL an inverted interlock must disagree somewhere` fires on 63 of 64 sweep
points.

**A shut gate assigns.** `0x82302EB0` and `0x82302EF4` store `0.0` rather than
letting the decay run. With a half-full secondary the two differ by 0.4 on the
very first frame.

## And five of the eight are the fields the step clears

Cycle 1371 read `0x82283898` zeroing `+360`, `+364`, `+304`, `+308` and `+312`
when bit 0 of `[this+332]` is set, before calling slot 33. Those are five of the
eight words this function writes.

That is the strongest evidence available that these *are* the control state
rather than scratch: a reset drops exactly them. No name was needed for it, and
none is used — the fields keep their offsets, because the class has no RTTI and a
guessed name would be a claim the port cannot support.

## The differential passed on its first run

```
flight_controls_microexec=pass cases=23 passed=23 values_compared=184
```

Twenty-three cases, 184 float values, bit for bit, and this function needed
**none** of the integrator's concessions: no mid-function entry, no epsilon
trick, no step cap, no skipped vector block. It runs from its own entry and
returns.

The one thing carried over from cycle 1373 was the fix, not the defect: every
input is rounded to single in the oracle before use, because that is what the
emulator is seeded with.

The port also had the `fnmsubs` fusion right only after being written twice —
`centre_axis` first took a pre-multiplied `rate * factor`, which rounds twice
where retail rounds once. Caught while writing, not by the differential, but it
is the same trap.

## Not established

- What the fields mean. The roles are derived — a held ramp, a gated second
  stage, a differential, three rate-limited axes — the *names* are not.
- Where `[+36]`…`[+52]` come from. They are the commands; nothing here writes
  them.
- Slot 32 (`0x82302C88`, 74 instructions, 6 calls, loads **π/180**) and slot 33.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 23 (1351–1371, 1374) |
| implementation/integration spent on A3.2 | 6 (1354–1356, 1372, 1373, 1375) |

The ratio moved because the method changed: bounding a function's footprint by
capsule before reading it costs one Ghidra run and removes most of the reading.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 16 behaviours
ctest                                 100% passed, 0 failed out of 35
tools/tests                           Ran 77 tests, OK
flight_controls_microexec             pass 23/23, 184 values
flight_step_microexec                 pass 10/10, 30 values
```

## Next

Slot 32, `0x82302C88` — 74 instructions, no vector, six calls, and it loads
**π/180**, so it converts degrees to radians somewhere. It runs immediately after
the integrator with the same float and the same position pointer. Bound its
footprint by capsule first; the six calls are the only new thing, and stubbing or
following them is a decision the probe will make cheap.
