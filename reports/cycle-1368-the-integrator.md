# Cycle 1368 — the integrator

## Qualification

- Ghidra was used to read two constants. **No oracle pass.**
- No product C++ changed, no contract changed.

## Found

`sub_82303110`, 359 instructions, `.pdata` agreeing:

```
lfs    f8,72(r30)
fmuls  f9,f0,f31
fmadds f9,f9,f13,f8         f9 * f13 + f8
stfs   f9,72(r30)
      ... and the same for 68(r30) and 64(r30)
```

**Three `fmadds`, one per component of a 3-vector at `+0x40`, `+0x44`, `+0x48`.**
Position plus rate times factor, stored back. An Euler step.

The two constants were read, not inferred:

- the factor is `0x82069B40` = **0.2777777910232544**, which is exactly **1/3.6**;
- the **middle component alone** is then clamped to a minimum of
  `0x82003214` = **10.0**.

`1/3.6` is the km/h-to-m/s factor. That is an observation about the number — what
is measured is that this constant multiplies a rate before it is added to a
position. And the clamped component is not called altitude here; what is measured
is which one is clamped and to what.

## How it was found, which is the part worth keeping

A3.2 spent sixteen cycles following data — the input tick, the transform, the
unit's vtable, the children, the arena, a placement branch — and every one of
those was bounded, answered, and not flight. Then:

```
1365  the naive vector signature                      461 functions -- useless
1366  li rN,64 + stvx128 + lvx128, LIVENESS tracked     10
1367  all ten read: nine copy, one differences         0 integrate
1368  the SCALAR form, all three components             3, and the first is it
```

Cycle 1367's complete negative is what made this cycle possible. Ruling out the
vector form over a defined population is what turned "look somewhere else" from a
guess into the next step, and cycle 1367 named the scalar form explicitly rather
than leaving it to be rediscovered.

Two refinements did the work: **tracking liveness** (1366) and **switching the
store width** (1368). Neither is clever; both are the difference between 461 and 3.

## Not established

- What `f31` is — it scales the rate before the factor, and nothing read says
  whether it is a time delta.
- What objects `r30` and `r31` are.
- What the other two all-three-component candidates, `0x8222EC18` and
  `0x82304AB8`, do.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

`f31`. It is the one term between a rate and a position that has no name, and if
it is the frame's elapsed time then this function is the flight integrator
outright. It is also exactly the kind of question the harness settles in one
capsule rather than by reading — which is what cycle 1343 did for the last float
this campaign refused to name.
