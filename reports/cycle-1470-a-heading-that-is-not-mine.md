# Cycle 1470 — a heading that is not mine

## Qualification

- **No Ghidra run and no oracle pass.** The product.
- Product C++: the flight-over-terrain test gains a second leg. ctest stays
  **58**.
- **No contract entry.**

## The coupling

Cycle 1469 established that the integrator wants a **unit direction** and a
**speed**. The direction it was getting was a unit vector I chose.

`step_flight_session` produces a basis from contracted arithmetic, and row 2 of
that basis is the forward vector. Feeding it straight in:

```
basis-driven: forward |len-1| <= 0.00e+00  travelled 20834  final (-1500, 400, 26834)
```

> **The basis's forward row is unit length exactly — `0.00e+00` over 3,000
> frames.**

Not "within tolerance": zero. Which is the strongest available statement that
row 2 and the integrator's direction parameter are the same object. The rotation
kernel and the integrator were derived in different cycles from different
addresses, and they fit without adjustment.

`travelled 20834` is `3000 × 1500 × kRateToStep / 60` to the unit — the leg moves
at exactly the speed the parameterisation says.

## What it does not show

The stick command was a **roll**, and the aircraft flew straight: `at64` never
left `-1500`. That is correct — a roll about the forward axis does not change
forward — but it means this leg tests the **coupling** and not the steering. A
turn needs yaw or pitch, and nothing here has produced one.

So: the heading is now retail's, the speed is still mine, and the *sequence* of
headings is untested.

## Not established

- Whether a pitch or yaw command turns the flight path as it should. The basis
  moves under those commands (`retail_flight_session_tests` shows it), but
  nothing has flown one.
- The speed. `[model+32]` is read from the model and clamped against
  `[model+1264]`; both are still supplied by hand.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 58
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Turn.** A pitch command flown for a few hundred frames should bend the path,
and the check is arithmetic rather than visual: the heading at the end differs
from the heading at the start by an angle, and that angle should match what the
rotation limits and rates predict. It is the first thing in this thread that
would test the flight model as a *trajectory* rather than as a sequence of
frames, and it needs nothing new.
