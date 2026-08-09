# Cycle 1388 — the rest of the slot

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_live_flight_axes.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 39**, was 38.
- `analysis/flight/live-flight-ramps-microexec.tsv` **replaced** by
  `live-flight-slot30-microexec.tsv`, which now compares all ten written words.
- **Contract: the twentieth behaviour**, `retail_live_flight_axes`.

## Slot 30 of the live model is now ported whole

Cycle 1387 took the ramp block. This takes the three axis blocks, and together
the two headers cover **every store** in `0x82303E68`.

## The rates are per axis and per regime

Retail copies two sixteen-byte blocks, `[+576]` and `[+592]`, to the stack and
multiplies six of the eight floats by the step. Element *i* of the first block is
axis *i*'s **command** rate; element *i* of the second is its **decay** rate. The
fourth element of each is copied and never used.

Each axis then chooses between two regimes on `|command|` against **2⁻¹⁶** — the
same word that gates the integrator's normalise and guards `atan2`. Below it the
axis decays toward zero without crossing; at or above it, it is driven.

## The drive is quadratic, and the limit is what stops it

For `+304` and `+312` the target is

```
cmd > 0:   cmd*cmd + cmd        fmadds f0,f0,f0,f0
cmd < 0:   cmd - cmd*cmd        fnmsubs f0,f0,f0,f0
```

times **2/3** — that is `sign(cmd) · (|cmd| + cmd²) · 2/3`, which reaches **4/3**
at full deflection. The axis then lags **1.5 times** the scaled gap toward it and
is clamped to [−1, +1].

So the target is *outside* the limit and the clamp is what bounds the axis, not
the curve. A port that used the command directly would be smooth, plausible, and
wrong on every deflection; the control `CONTROL a linear target must disagree`
fires on all 64 sweep points.

## `+308` is a different animal, and it carries the timers

No curve and no lag: it steps by its command rate in the direction of the sign,
ignoring the magnitude entirely. And it maintains **two hold timers**, `+1352`
and `+1356`, one per direction — incremented by the step while that direction is
held, decayed at **ten times** the step otherwise, floored at zero.

The two halves are **independent `if/else` pairs** in retail, not one three-way
branch, so on a positive command the positive timer accumulates *and* the
negative one decays in the same frame. Nothing in this function reads them.

And because both pairs live inside the driven branch, a command below the epsilon
leaves both timers **untouched** — neither accumulating nor decaying.
`an_undriven_at308_leaves_its_timers_alone` pins that, and it is the kind of
detail that a rewrite for tidiness would lose.

## The differential

```
live_flight_ramps_microexec=pass cases=17 values_compared=170
```

Seventeen cases, **all ten written words compared**, passing on the first run.
Five new cases cover the axes: all three driven, all three decaying, all three
saturated by a huge step, the timers running in opposite directions, and exactly
`2⁻¹⁶` on the boundary.

The audit was **extended rather than rewritten** — five columns moved from
"recorded" to "compared" and the oracle gained the axis arithmetic, which is what
cycle 1387 predicted it would take.

## Not established

- What reads `+1352` and `+1356`.
- The fourth element of each rate block.
- Slots 32 and 39 of the live model, and its step `0x82306A38`.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 10 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387, 1388) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 20 behaviours
ctest                                 100% passed, 0 failed out of 39
tools/tests                           Ran 77 tests, OK
live_flight_slot30_microexec          pass 17 cases, 170 values
```

## Next

`0x82306A38`, the live model's step — 123 instructions, no vector, and it
dispatches the slot 30 just contracted plus slots 31, 32 and 39, then the two
functions the other step also calls. A composite differential of the same shape
as cycle 1381's is available immediately: slot 30 live through a real dispatch,
the rest stubbed, compared against the composed port.
