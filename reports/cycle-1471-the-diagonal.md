# Cycle 1471 — the diagonal

## Qualification

- **No Ghidra run and no oracle pass.** The product's own contracted arithmetic.
- Product C++: a test added. ctest **58 → 59**.
- **No contract entry.** Nothing new is ported; what is added is a pairing
  between two already-contracted things, established by intervention.

## The control matrix

Fly each stick axis alone for 600 frames, double each rotation limit alone, and
measure the angle the basis's forward row turns through:

| stick | baseline | ×2 `at1248` | ×2 `at1252` | ×2 `at1256` |
|---|---:|---:|---:|---:|
| **12** | 7.0819 | **14.1723** | 7.0819 | 7.0819 |
| **13** | 0.9171 | 0.9171 | **1.8356** | 0.9171 |
| **14** | 0.0000 | 0.0000 | 0.0000 | 0.0000 |

The diagonal doubles; every off-diagonal is unchanged **to the last digit**.

> **Stick 12 drives `at1248`, the row-0 rotation (`0x820A99F8`).
> Stick 13 drives `at1252`, the row-1 rotation (`0x820A9B30`).**

Six negative cells, each an exact equality, are what makes those two positives a
pairing rather than a correlation.

## And the third row is the control on the other two

Stick 14 turns the forward row by **exactly zero** — not 1e-7, zero — under every
limit including its own. That is what a rotation *about* row 2 must do to row 2.

So `FlightRotationLimits`'s comment that `at1256` is the row-2 rotation is
confirmed by behaviour, and the row convention `demo_flight_view.cpp` calls "my
choice" is at least self-consistent: whatever row 2 is named, it is the axis
stick 14 rotates about, and it is the one the integrator takes as its direction
(cycle 1470, unit to `0.00e+00`).

None of this names an axis "pitch". A rotation limit is a derived pairing; a
cockpit word is not.

## A guess made and killed in the same cycle

The two turn rates differ by a factor of about 7.6, and `FlightModelConfig` has a
`row0Divisor` of **7.0**. I tested it at 7.0, 14.0 and 3.5:

```
row0Divisor  7.0 -> 0.9171 deg      14.0 -> 0.9171      3.5 -> 0.9171
```

Identical to four decimals. `row0Divisor` has nothing to do with it, and the 7.6
is a coincidence of two independent limits. `rates308` moves the result by 0.8%
— the ramp, not the rate.

## Not established

- What sets the *ratio* between the axes beyond the limits themselves.
- Whether the limits are radians per second or something scaled. The doubling is
  exact, so they are linear in whatever they are.
- Any cockpit name for any axis.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Fly a turn over the map.** The three pieces now fit: a stick axis bends the
basis, the basis's row 2 is a unit direction, and the integrator takes a
direction and a speed. A curved path over the real heightfield is the first
trajectory this campaign would have produced end to end from contracted parts,
and it is the thing a video shows.
