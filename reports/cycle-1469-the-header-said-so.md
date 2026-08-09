# Cycle 1469 — the header said so

## Qualification

- **No Ghidra run and no oracle pass.** The product's own headers.
- Product C++: a test corrected. ctest stays **58**.
- **No contract entry.**

## Correcting cycle 1468, which asked a question already answered

That cycle recorded, under *not established*:

> "What `rate_scale` is for, given the integrator already applies `kRateToStep`.
> The header does not say and this cycle did not read it."

**The header says.** `retail_flight_session.h`, from cycle 1415:

> "retail's rates are a **DIRECTION**, normalised to unit length behind that
> boundary, and `rate_scale` — `[model+32]`, clamped against `[model+1264]`
> above and 0.0 below at `0x82303530..0x82303554` — is the **SPEED** that scales
> it."

Twenty-five lines of derivation, with addresses, in the file I was calling. I
wrote "the header does not say" and published it as an open question.

## Which makes cycle 1468's flight a category error

It passed a magnitude-5400 vector as the "direction" and `kRateToStep` as the
"scale". Neither is what the parameter is. The trajectory it produced was real —
the integrator is contracted and does what it does — but the setup described no
aircraft.

Corrected: a **unit** direction, and `rate_scale = 1500`, which `kRateToStep`
divides by 3.6 to about 417 world units a second. 6000 ticks is 100 seconds and
covers 41,600 units.

```
ticks 6000  at68 60.00  highest ground 171.96
lowest clearance -111.96  clear 3124  below 2876
```

The finding is unchanged and now rests on a coherent setup: **the contracted
integrator, with retail's constant 10.0 floor, spends 2,876 of 6,000 ticks below
the ground under it.**

## Why the error survived a cycle

The misuse produced **6.94 units a tick**. The correct parameterisation, at a
plausible aircraft speed, produces **6.94 units a tick** — because
`5400 × kRateToStep² / 60` and `1500 × kRateToStep / 60` are the same number.

So the wrong call flew at a right-looking speed. Cycle 1468 spent three runs
chasing why the aircraft "never reached land" and concluded the scale was
mysterious, when the number it was getting was the one it should have wanted from
a correct call. **A wrong parameterisation that lands on a plausible value does
not announce itself**, and the only thing that would have caught it is the one
thing not done: reading the parameter's documentation before passing something to
it.

## Not established

- Whether `[model+32]` is the speed a player controls, or a cap. It is read from
  the model and clamped against `[model+1264]`; which of those is throttle and
  which is limit was not asked here.

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

**Give the flight a direction it did not invent.** The direction is now unit and
the speed is now a speed, but both are still mine. `step_flight_session` produces
a basis from contracted arithmetic, and row 2 of that basis *is* a normalised
forward vector — the same shape the rates want. Feeding the basis's own forward
into the integrator would make the heading retail's too, and leave only the
speed invented.
