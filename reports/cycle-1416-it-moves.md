# Cycle 1416 — it moves

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass, and no Ghidra run.**
- ctest stays **51**. **No contract entry** — the demo ports nothing; it composes
  what cycle 1415 wired.
- New capture `reports/mission01-native-captures/demo-bare-flight/` with
  `bare-flight.mp4`, seven stills, README and CAPTION.

## The deliverable

Thirty seconds of flight, 900 frames at 30 fps. The aircraft climbs from 1000
units to 1489 and travels 7478 downrange while the stick pitches, rolls, and
centres. **It is the first capture in this campaign where the aeroplane moves**,
and the movement is the contracted integrator `0x82303110`, not a rendering
trick.

## Two numbers that make that checkable

**The integrator is exact.** Flown with no pitch at all, thirty seconds at
900 km/h gives `at72 = 7499.899`. 900 ÷ 3.6 × 30 = 7500. Retail's `1/3.6` from
`0x82069B40` is doing the arithmetic and the answer is the arithmetic's.

**The floor is retail's and it fires.** With the pitch increment at `+1.0` the
aircraft descends from 1000 and stops at `at68 = 10.000` exactly — the `10.0`
at `0x82003214`, a minimum altitude nobody in this campaign chose. The first
run of this cycle hit it by accident and read as a bug; it is the clamp working.

## The renderer had to stop being the world

`draw_flight_view` drew the ground at `-invented_altitude` from an eye fixed at
the origin. Every point is now drawn **relative to the eye**, the ground plane is
world `y = 0`, and the altitude comes from `position.at68`.

Two things guard that change:

- the **stationary overload** is kept and delegates with
  `{0, invented_altitude, 0}`, which is the same geometry;
- and the nine committed stills regenerate **byte-identical**, 9 of 9. A
  refactor of a renderer that claims to change nothing should be made to prove
  it, and this one is cheap to prove.

The grid is snapped to the eye, quantised to its spacing. Without it a moving
aircraft flies off a finite grid in about ten seconds. It is a rendering choice
and it is in the README's invented list.

## What the capture taught, and it was not what I expected

Driven with pitch **targets** of `+0.8` and `-0.8`, the two runs produced
byte-identical trajectories — `at64 = 238.491`, `at72 = 7341.059`, both.

That is not a wiring bug. Retail's setters compare `|current - target|` against
one degree and, when that passes, store the target and add the **increment**
(cycle 1406). **The target only decides whether a command is taken at all; the
increment is what flies the aeroplane.** The loop drives `increment12` now, and
the sign behaves: `-1.0` climbs to 2304, `+1.0` descends to the floor, `-0.4`
gives the gentle climb the video uses.

I had driven three cycles of trajectories off the target without noticing,
because until this cycle nothing in the demo depended on the *sign* of the
result — attitude changes look plausible either way. A moving aeroplane does not.

## The invented list, honestly larger

Cycles 1409 and 1412 shrank this list. This cycle **grows** it, and that is the
correct direction given what cycle 1415 established:

> **the heading and the speed.** Retail's integrator is fed a unit direction from
> a vector normalise seeded on `vrsqrtefp` and `vrefp`, and a speed from
> `[model+32]`. Neither is obtainable. Here the direction is basis row 2 and the
> speed is 900 km/h. **The heading is chosen; the flying of it is not.**

The gravity bias is passed as `0.0`, and the README says why in one sentence
rather than leaving it to be inferred: retail's is `f25*f24`, `f24` is
unresolved, and passing `f25` alone would both assume `f24 = 1` and sink the
aircraft, because the lift that balances gravity is not in the contracted chain.

## The caption, corrected again

It said *"The aircraft changes attitude and does not move: its position step
depends on the same estimates."* Both halves were wrong by cycle 1415: it moves,
and the function that clause blamed — `0x823042D0` — is the live model's own
step, not the contracted integrator.

The test that asserted the old sentence now asserts its **absence**, plus the two
clauses that replace it. A caption test that only checks for presence lets a
stale claim survive; this one fails if the aircraft stops moving *or* if the
sentence comes back.

## Not established

- Which of `at64` and `at72` is north and which is east. Unclaimed in the README.
- Whether the pre-normalise stack vector at `0x82303464`..`0x82303470` is
  reachable without the estimates — cycle 1415's open question, and the one that
  would shrink the invented list again.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
committed stills regenerate           9 of 9 byte-identical
```

## Next

`tools/audit_capture_images_match_metrics.py` does not cover this directory —
it reproduces a renderer colour hash against recorded metrics, and this capture
records none. Either give it metrics it can check or say in the README that it
is unchecked; an unchecked capture beside seven checked ones is the shape cycle
1273 was caught by.

Then the JV decision, which is now the only thing between this and a
recognisable Mission 01.
