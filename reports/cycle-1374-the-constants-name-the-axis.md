# Cycle 1374 — the constants name the axis

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus for mnemonics, the
  image for fifteen float words.
- **Product C++ changed**: `retail_flight_step.h`, `retail_flight_step_tests.cpp`.
  ctest stays 34; the flight-step suite gains one case.
- New artefact `analysis/flight/flight-integrator-constants.tsv`.

## What was done instead of another search

Cycle 1373's "next" was to extend the capsule upward. The cheaper first move was
to resolve **every** `lfs` in `sub_82303110` against its `lis`/`addi` base — one
pass, fifteen constants — rather than chase one register at a time.

| address | value | identity |
|---|---|---|
| `0x82069B40` | 0.2777777910232544 | **1/3.6** |
| `0x8200F308` | 2.722222328186035 | **9.8/3.6** |
| `0x82069C2C` | 1.52587890625e-05 | **2⁻¹⁶** |
| `0x82069E70` | 0.2617993950843811 | π/12 |
| `0x82008AD8` | 0.3183099031448364 | **not** 1/π — see below |
| `0x8200F300` | 0.1111111119389534 | 1/9 |
| `0x8200F304` | −45.0 | |
| `0x82069E3C`, `0x82007D5C`, `0x82069CA0`, `0x82002FD4` | | 1/30, 1/15, 1/5, 1/10 |
| `0x82069C7C`, `0x8200134C`, `0x82003214`, `0x8200082C` | | 30.0, 3.0, 10.0, 0.0 |

## `at68` is the vertical component

Cycle 1372 wrote this as open: *"Which component is which axis. `at68` carries
the 10.0 floor, which is what an altitude would carry; that reading is written in
the header as a reading."*

It is now derived, on two independent grounds:

1. It is the **only** component with a floor, and the floor is 10.0.
2. Its bias is a **gravity term**. `f25` is built at `0x82303300`:

```
lfs   f0,344(r31)          a model field
fcmpu cr6,f0,f26           against 0.0
lfs   f25,-3320(r11)       9.8/3.6, at 0x8200F308
beq   -> keep it
fmuls f0,f0,30.0           otherwise scale by [model+344] * 30
fmuls f25,f0,f25
```

and only `at68` has `f25*f24` subtracted.

## And the unit is settled

Cycle 1372 said of 1/3.6: *"1/3.6 is the km/h→m/s conversion… That is **an
interpretation, and it is written here as one** — the constant is measured, the
unit assignment is not."*

The same function loads **`float32(9.8/3.6)` exactly** — and **not**
`float32(9.81/3.6)`, which is 2.7249999 and would have been just as natural a
choice for a programmer who meant something else. The image carries *g* and the
km/h→m/s divisor as a matched pair inside one function, and the ratio of the two
stored words is 9.8.

`step` is seconds; the rates are km/h. That is now evidence, and it is pinned by
a test rather than left in prose, so a later "simplification" of either constant
breaks the suite.

## A constant that is a trap

`0x82008AD8` = **0.3183099031448364**.

`float32(1/π)` is **0.31830987334251404**. They are not the same word.
`float32(0.3183099)` *is* — so this is a **seven-digit decimal literal someone
typed**, not a correctly-rounded reciprocal. A port writing `1.0f/M_PI` is one
ulp off on every use.

The same function loads `π/12` at `0x82069E70` **correctly rounded**. Mixed
provenance inside one function, which is what real code looks like and why
"it's obviously 1/π" is not a substitute for the comparison.

## The class keeps its constants against its vtable

`0x8200F300`, `0x8200F304` and `0x8200F308` — 1/9, −45.0 and 9.8/3.6 — are the
three words **immediately after** the class's 36-slot vtable:
`0x8200F270 + 0x90 = 0x8200F300`.

That is a general handle. Cycle 1370 counted 306 vtables with no RTTI; for any of
them, the words just past the last slot are that class's own constant pool, and
they are often more legible than the code that uses them.

## Four stale statements in my own header, corrected

Writing this cycle's findings into `retail_flight_step.h` put it in contradiction
with itself in four places, all from earlier cycles, all now fixed:

- *"THE CONSTANTS ARE TWO WORDS OF THE IMAGE"* — there are fifteen.
- *"that reading is not derived"* about the vertical axis — it is now.
- *"a value in f26 that this cycle did not read"* — `f26` is 0.0, at
  `0x8200082C`. The clamp above the window is a floor at zero, and the port does
  not enforce it because the port does not claim that instruction.
- *"f11 was not read"* — it is 2⁻¹⁶.

A header that documents its own derivation goes stale exactly as fast as the
derivation improves, and a stale derivation is what the artefact checker cannot
see.

## Not established

- `f24`: `frsp(f1) * 0.3183099` at `0x82303278`, where `f1` is returned by a call
  this cycle did not follow. `mid_bias` therefore stays an argument.
- `[model+344]`, the field that scales gravity.
- The direction output and its VMX normalise.
- What calls the slot-8 setter, hence what `[model+112]` points at.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 23 (1351–1371, 1374) |
| implementation/integration spent on A3.2 | 5 (1354–1356, 1372, 1373) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 15 behaviours
ctest                                 100% passed, 0 failed out of 34
tools/tests                           Ran 77 tests, OK
```

## Next

The other two pure virtuals the step calls with the same float and the same
pointer: slot 30 at `0x82302DB0` and slot 32 at `0x82302C88`. They run
immediately before and after the integrator on every frame, they write into the
same block, and neither has been read. Resolving their constants first — the
move that made this cycle cheap — is the way in, and the class's pool at
`0x8200F300` is already mapped.
