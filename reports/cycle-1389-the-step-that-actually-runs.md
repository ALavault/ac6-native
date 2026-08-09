# Cycle 1389 — the step that actually runs

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_live_flight_step.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 40**, was 39.
- New tool `tools/audit_live_flight_step_microexec.py`, new artefact
  `analysis/flight/live-flight-step-microexec.tsv`.
- **Contract: the twenty-first behaviour**, `retail_live_flight_step`.

## What runs every frame

`0x82306A38` is slot 15 of `0x8200F310`. Cycle 1379 showed this branch overrides
the base class's slot 11 with the empty `blr` and is driven from slot 15 instead;
cycle 1384 showed this branch is the one the entity drives. So this is the flight
model's per-frame entry point on the aeroplane.

```
call slot 30 (this)                       0x82303E68  contracted
call 0x82283168 (this, f1, f2 = [+376])
call 0x822831E8 (this, f1, r5 = this+304)
unless bit 7:  slot 39 or (slot 31 then 0x82304AB8), then slot 32
call 0x82282938 (this, f1, r5)
bit 1 ? slot 33 : decay [+412] and [+408]
call 0x82281C18, 0x82282E20, 0x82283480, 0x82326FE8
```

**Two things it shares with the other step**: `0x82282938` and `0x82326FE8`, in
the same positions relative to the virtuals; and the reset is **bit 1** in both,
selecting slot 33 in both. The two drivers are variants of one design, which is
why cycle 1381's reading carried over.

**And one thing it does not**: it calls the performance lookup `0x82283480`
**every frame**. Cycle 1378 said that happened only on reset and cycle 1383
corrected it; this is the function that shows it.

## Bit 7 does two jobs, and the first run is what found it

The `skip-attitude` case **faulted at 34 steps** — inside slot 30, on a flag the
step was supposed to be handling.

Bit 7 is the step's skip-attitude flag **and** slot 30's bypass, which dispatches
slot 38. A vtable without slot 38 filled cannot survive it. One bit, two uses, in
two different functions, and no amount of reading `0x82306A38` would have shown
it: the second use is in a callee, behind a virtual dispatch, keyed on a field
the caller loaded for its own purposes.

## The composite

```
live_flight_step_microexec=pass cases=5 values_compared=10
```

The second composite differential of the campaign. The real slot 30 runs through
a real dispatch; everything else is stubbed **at its genuine address**.

The **stubbed-call count is part of the comparison**, and it is what makes the
flag claims evidence rather than reading:

| case | flags | stubbed calls |
|---|---:|---:|
| plain | 0x00 | 10 |
| skip-attitude | 0x80 | 8 |
| slot39 | 0x40 | 9 |
| reset | 0x02 | 11 |
| skip-and-reset | 0x82 | 9 |

Five distinct branch structures, five distinct counts, and the two that coincide
do so for different reasons.

## The decays, and an epsilon that means the opposite of the neighbouring one

`[+412]` and `[+408]` each decay by `value × step × 3` — exponential, fused —
**and only while `|value| ≥ 2⁻¹⁶`**. Below that the field is **left alone**.

That is the opposite of what the same constant does one function away: in slot
30's axis blocks, `|command| < 2⁻¹⁶` selects the decay regime. Here it *skips*
the decay. Same word, adjacent code, inverted role — and
`below_the_epsilon_the_field_is_left_alone` fails if they are conflated.

## Not established

- `0x82283168`, `0x822831E8`, `0x82304AB8`, `0x82281C18`, `0x82282E20` — five
  direct calls, all unread.
- Slots 32 and 39 of the live model.
- What reads `[+408]` and `[+412]`.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 11 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387–1389) |

Four of the last five cycles ended with a contracted behaviour, all of them on
the model that flies.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 21 behaviours
ctest                                 100% passed, 0 failed out of 40
tools/tests                           Ran 77 tests, OK
live_flight_step_microexec            pass 5 cases, 10 values
```

## Next

`0x822831E8`, called with `r5 = this+304` — the three axes slot 30 has just
written. It is the first consumer of the contracted control state, and it is a
direct call rather than a virtual, so its callers and its footprint are both
enumerable. Bound it by capsule first.
