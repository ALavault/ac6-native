# Cycle 1415 — the rates are a direction

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass, and no Ghidra run** — this is a static
  read of the recompiled corpus plus product C++.
- ctest stays **51**; two new cases inside an existing binary. **No contract
  entry** — `retail_flight_session` composes contracted behaviours and ports
  nothing, which is unchanged.

## The question, and it needed asking before the wiring

Cycle 1414 closed by naming one thing to read rather than assume: **what the
integrator's rates mean physically**, because a demo that flies sideways would
be a wiring choice presented as retail's arithmetic.

They are a **direction**, and `rate_scale` is a **speed**.

## How that is established

The three rates are not computed near where they are used. They are *loaded*:

```
0x82303524  lfs f9,88(r1)        the three rates come off the STACK
0x82303528  lfs f10,84(r1)
0x8230352C  lfs f12,80(r1)
```

and those slots are written by a `stvx128` at `0x82303520`, the tail of a vector
block that begins at `0x823034AC` by loading *the same three slots*:

```
0x823034BC  lvx128       v12,r0,r11      r11 = r1+80
0x823034C4  vmsum3fp128  v0,v12,v12      |v|^2
0x823034CC  vrsqrtefp    v13,v0          <- ESTIMATE
   ... two Newton steps, a zero guard ...
0x823034FC  vrefp        v0,v0           <- ESTIMATE
   ... two more Newton steps ...
0x8230351C  vmulfp128    v0,v12,v0       the original vector times 1/|v|
```

A read-modify-write that **normalises the vector in place**. And immediately
after the rates are read, `[model+32]` — clamped against `[model+1264]` above and
against `0.0` below at `0x82303530..0x82303554` — multiplies all three:

```
0x82303560  fmuls f12,f0,f12
0x82303568  fmuls f10,f0,f10
0x82303570  fmuls f0,f0,f9
```

Unit direction times a scalar. So `rate_scale` carries the **speed in km/h** —
which closes the loop with cycle 1372's unit finding, where `1/3.6` and `9.8/3.6`
were shown to be a matched pair.

## An addition to cycle 1383, not a correction

Cycle 1383 established that the integrator's normalise is guarded on all three
scaled rates being below 2⁻¹⁶, that the gravity bias makes the middle one
non-zero every frame, and that the position stores nonetheless **precede** it —
so `retail_flight_step`'s contracted arithmetic is unaffected. All of that
stands.

What it did not say is that there are **two** normalise blocks:

| block | at | estimates | what it gates |
|---|---|---|---|
| first | `0x823034AC`..`0x82303520` | `0x823034CC`, `0x823034FC` | **the rates themselves** |
| second | after the stores | `0x8230360C`, `0x8230363C` | the direction output at `[model+128/132/136]` |

Cycle 1383 characterised the second. The first sits *before* the rates are read
and is guarded differently — not on 2⁻¹⁶ but on a comparison computed at
`0x82303490`, `r11 = (f0 < f11) ? 1 : 0`, whose two operands this cycle did not
trace.

The consequence is the one that matters here: **retail's rates are behind an
estimate boundary on at least one path.** The contracted integrator is usable;
its input is not obtainable.

## So the wiring says so, out loud

`FlightSessionState` gains a `FlightPosition position`, and the integration is a
**second entry point** rather than a line inside `step_flight_session`:

```cpp
void integrate_session_position(FlightSessionState& state, FlightRates rates,
                                float rate_scale, float mid_bias, float step);
```

Everything `step_flight_session` composes is contracted retail arithmetic driven
by contracted retail inputs. This one is not, and separating them is the point:
a caller has to name a direction and a speed, and the signature makes that an
argument rather than a default buried three files down.

What the caller gets in exchange is that the integration, the `1/3.6` scale, the
`10.0` floor, the gravity bias and the fusing are all retail's.

## Two tests, and one of them is a control

- one second at 360 km/h along the first component moves ~100 units, and moves
  nothing else;
- the vertical component floors at **10.0** after a long descent;
- **CONTROL**: the same descent on `at64` must *not* floor. A port that spread a
  clamp retail applies to one axis across all three passes the first two and
  fails this one.
- the gravity bias pulls `at68` down and leaves `at64` and `at72` bit-identical.

## A stale claim of my own, corrected

`retail_flight_session_tests.cpp` emitted a trajectory whose header read:

> POSITION IS ABSENT, and not by omission: the live model's position step is
> `0x823042D0`, which cycle 1383 showed depends on `vrefp` and `vrsqrtefp`.

`0x823042D0` is the **live model's own slot 31**, a different function from the
contracted integrator `0x82303110`. Two blockers were being run together: one
real (the live model's aerodynamics) and one that does not apply (the integrator,
which is ported). The header now says which is which.

The demo's caption still says the aircraft does not move, and that is still true
*of the demo* — nothing there calls the new entry point yet.

## Not established

- What `f0` and `f11` are at `0x82303490`, and therefore which path the game
  takes through the first normalise.
- Whether the pre-normalise stack vector at `0x82303464`..`0x82303470` is
  reachable without the estimates. If it is, retail's own direction becomes
  obtainable and the demo's invention shrinks again.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 79 tests, OK
contract_addresses                    pass cited=285 supported=285
```

## Next

Wire the demo through `integrate_session_position` and emit a moving trajectory:
take a basis row as the direction — the invention already declared in the caption
— pick a speed, and let the contracted integrator do the rest. Then the camera
follows the aircraft instead of watching it rotate in place, and the frames
become a sequence worth encoding rather than nine stills.
