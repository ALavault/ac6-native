# Cycle 1376 — the controller and the kernel are the same code

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays 35.
- New tool `tools/audit_flight_math_seams.py`, new artefacts
  `analysis/flight/slot32-orientation-chain.tsv` and
  `analysis/flight/flight-math-seams.tsv`.
- **No contract entry** — see below.

## Slot 32 is the orientation update, and it closes A3.1

`0x82302C88` — the third pure virtual the step calls, with the same float and
the same position pointer the integrator gets — applies **three rotations**:

```
0x820A9B30   from [model+308],  limit [model+1252], scale 1/15
0x820A99F8   from [model+304],  limit [model+1248], via 0x82302B78
0x82211828   from [model+312],  limit [model+1256], scale 2/3
```

Those are **exactly** the three functions `retail_transform.cpp` already ports,
in the order `retail_transform.h` already records:

> rotate about row 1 → `0x820A9B30`, called **FIRST**
> rotate about row 0 → `0x820A99F8`, called **SECOND**
> rotate about row 2 → `0x82211828`, called **THIRD**

A3.1 derived that order from `0x822A1E80` — a *different* caller, in a different
subsystem, read eight cycles earlier. The flight controller uses the same kernel
in the same order.

That is the thing the campaign's steering required and refused to assume: **one
shared transform kernel, not two independent substitutes.** It is now measured
rather than intended.

## And the three control axes are the three angle sources

Slot 30 (`0x82302DB0`, contracted last cycle as `retail_flight_controls`) writes
`+304`, `+308` and `+312`. Slot 32 uses each as the multiplier of **exactly one**
rotation.

Neither function says this on its own. Slot 30 produces three interchangeable-
looking numbers; slot 32 consumes three fields. The assignment exists only in the
pair, and each axis's per-aircraft limit is a separate field — `[+1248]`,
`[+1252]`, `[+1256]`, three consecutive floats.

## Then it extracts Euler angles, and both seams are certified

```
[model+16] = -asin(clamp([pos+52], -1, +1))      via 0x82380570
[model+24] =  atan2([pos+20], [pos+36])          via 0x820936E8
[model+20] =  atan2([pos+48], [pos+56])          via 0x820936E8
```

`0x82380570` clamps to asin's domain and uses `fsqrt`; `0x820936E8` takes two
arguments and guards both against 2⁻¹⁶. Those are *shapes*, and a shape is not an
identity — so they were measured, the way `XMScalarSinCos` was at cycle 1307.

```
asin    cases=20  worst=0 ulp   -> identical
atan2   cases=22  worst=0 ulp   -> identical
```

**Bit for bit with libm**, across asin's whole domain including both endpoints
and all four atan2 quadrants. `std::asin` and `std::atan2` are legitimate
substitutes in a port — with one exception.

## The exception, and the first run found it by being wrong loudly

The first atan2 run reported:

```
atan2  cases=18  worst=1061752795 ulp
```

That number is not a rounding disagreement; it is a different function on a
subdomain. Sixteen of eighteen cases were exact and two were not — the two where
**both** arguments are tiny.

`0x820936E8` opens with a guard: when `|a|` and `|b|` are *both* below 2⁻¹⁶ it
returns **zero** instead of an angle. `std::atan2(1e-6, 1e-6)` is π/4; retail is
0.

An unguarded substitution would put a **45-degree error into the aircraft's
orientation on exactly the frames where it is level** — the most common frame in
the game, and the one least likely to be caught by eye. The tool now models the
guard, pins the boundary at `1.52587890625e-05` from both sides, and both seams
come back at 0 ulp over 42 cases.

Worth naming the reason this was visible: the verdict is the **worst** case, not
an average, and it is printed in ulp rather than as pass/fail against a
tolerance. A tolerance of 1e-3 would have called eighteen-of-eighteen a pass.

And 2⁻¹⁶ is `0x82069C2C` — the same word cycle 1374 identified as the vector
normalise epsilon in the integrator. One threshold, two uses.

## Why there is no contract entry

Slot 32 itself has no micro-execution differential. It calls six functions, two
of them 35-instruction VMX128 routines, so a capsule needs either the register-
file bridge or a decision about stubbing — neither taken this cycle.

The plan's rule holds: no flight behaviour at the contract without a
`microexec`. The sixteen behaviours stand.

What *is* established and pinned is the chain and the two seams, which is what
the eventual entry will be built from.

## Not established

- Slot 32's own differential.
- `0x822A6400`, which scales the row-0 rotation when bit 3 of `[model+332]` is
  set.
- Where `[model+1248/1252/1256]` come from — they are per-aircraft limits and
  should trace back to the profile the plan names at `0x820A8678`.
- Slot 33, and `0x82282938` which the step calls after all three.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 24 (1351–1371, 1374, 1376) |
| implementation/integration spent on A3.2 | 6 (1354–1356, 1372, 1373, 1375) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 16 behaviours
ctest                                 100% passed, 0 failed out of 35
tools/tests                           Ran 77 tests, OK
flight_math_seams                     pass 42 cases, asin 0 ulp, atan2 0 ulp
```

## Next

`[model+1248/1252/1256]`. They are the three per-aircraft rate limits, they are
consecutive, and the plan already names `0x820A8678` as where the aircraft comes
from the profile. Finding the writer of those three floats connects the flight
model to the aircraft data — which is the last structural link between a
contracted controller and an aircraft that flies like the one the player chose.
