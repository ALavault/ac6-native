# Cycle 1390 — the first consumer of the control state

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_rate_servo.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 41**, was 40.
- New tool `tools/audit_flight_rate_servo_microexec.py`, new artefact
  `analysis/flight/flight-rate-servo-microexec.tsv`.
- **Contract: the twenty-second behaviour**, `retail_flight_rate_servo`.

## The chain now joins up

`0x822831E8` is a **direct** call from the live step, made immediately after slot
30, and it reads `+304`, `+308` and `+312` — the three control axes
`retail_live_flight_axes` produces — and writes three **rates** at `+144`, `+148`
and `+152`.

So the contracted chain is now: step → control surfaces → **rates**. Three
functions, three contracts, and the state flows between them rather than each
being an island.

## What it computes

```
target   = axis * limit
gap      = target - rate                            fmsubs, fused
gain     = |axis| >= 2^-16 ? driven * saturation : centred
rate    += gain * gap * step                        fmadds, fused
clamp to [-limit, +limit]
```

with

```
saturation = clamp(|rate / limit| + bias, 0, 1)
```

## Three things a plausible port gets wrong

**The biases are three different constants** — 0.5, 0.8 and 0.4, from
`0x82001354`, `0x82069ECC` and `0x82069CB4`. Not one used three times. The
control `CONTROL one shared bias must disagree` fires on 22 of 33 sweep points.

**The saturation scales the DRIVEN gain, not the centred one.** `bne` on
`|axis| < eps` jumps to the centred branch, which uses its gain unscaled. So a
saturated aircraft loses authority *while the stick is deflected* and recovers it
at neutral — the opposite of a damping term, and the opposite of what one would
write from the shape.

**All three saturations are computed before any servo runs.** Each reads its rate
before any has been updated, so a per-axis loop that interleaved them would use a
rate retail has not yet written. The port keeps the two passes separate and says
why.

## A degenerate limit reads a block of zeros

When `|limit| < 2⁻¹⁶` retail does not compute the normalised magnitude at all: it
reads it from a sixteen-byte block at `0x826EB940`, which is **all zeros**. The
saturation collapses to the bias alone.

That is modelled as zero rather than as a division guard, because it is what the
image contains and not what would be sensible. A port that wrote
`limit == 0 ? 1.0f : …` would be defensible and different.

## And a dead argument, named

The step passes `r5 = this+304`. **The function never uses it** — every read is
off `r3`. Saying so in the header is cheaper than a later reader assuming it
matters.

## The differential

```
flight_rate_servo_microexec=pass cases=9 values_compared=27
```

Nine cases, passing on the first run: driven and centred, the saturation clamp,
the degenerate limit, both rate clamps, axis isolation, three distinct answers
from equal inputs, and full mantissas. Nothing stubbed, nothing capped — 166
instructions with no call and no vector run end to end.

## Not established

- `0x82283168` (32 instructions, reads `+964`/`+968`, writes `+380`, calls
  `0x82283018`), `0x82304AB8`, `0x82281C18`, `0x82282E20`.
- What reads `+144`, `+148`, `+152`.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 12 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387–1390) |

Five consecutive cycles ending with a contracted behaviour, all on the model that
flies.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 22 behaviours
ctest                                 100% passed, 0 failed out of 41
tools/tests                           Ran 77 tests, OK
flight_rate_servo_microexec           pass 9 cases, 27 values
```

## Next

**What reads `+144`, `+148` and `+152`.** They are the rates the servo produces
and the aeroplane's actual angular velocity is presumably built from them; the
search is a displacement scan bounded to this class family, which is the shape
that worked at cycle 1378 and failed when unbounded.
