# Cycle 1387 — the first port of the model that flies

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_live_flight_ramps.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 38**, was 37.
- New tool `tools/audit_live_flight_ramps_microexec.py`, new artefact
  `analysis/flight/live-flight-ramps-microexec.tsv`.
- **Contract: the nineteenth behaviour**, `retail_live_flight_ramps`.

## The first behaviour ported from the model that actually runs

Cycle 1384 established that the entity's `0x8200F270` instance is never
addressed and the live model is the `0x8200F310` branch. This is that branch's
slot 30, and it is the first piece of A3.2 ported from the model that flies.

## A first-order lag, not a ramp

The contracted `retail_flight_controls` moves its ramps at a fixed rate per
second. This one moves them a **fraction of the remaining distance**:

```
[+360] += ([+48] - [+360]) * ([+952] * step)
```

with the coefficient carried **per model** at `+952` and `+956` rather than
being a constant in the code. That is an exponential approach — and there is **no
clamp on either lag**, so at `rate × step > 1` it overshoots the target and
retail lets it. The test `a_large_step_overshoots_because_there_is_no_clamp` and
the differential's `overshoot` case both pin that, because a port that helpfully
clamped would look more correct and be wrong.

The two secondary ramps then move by a **whole step** up or down depending on
whether their primary is past `[+404]`, bounded to `[0, 1]`. `+368` alone is
gated by a **byte** at `+1224` that is read *after* its update, so a zero byte
discards the step just taken rather than preventing it. `+372` has no such gate.

Bit **7** of `[+332]` bypasses the whole block and dispatches slot 38 instead —
and `+376` is still recomputed afterwards, because `0x82303FAC` is past the
branch join.

## The slice is a complete unit, and that was verified

The function writes ten words. `include/ac6/retail_live_flight_ramps.h` models
five of them.

That is not a prefix taken for convenience: **every store to `r31` was listed in
address order**, and `+360`, `+364`, `+368`, `+372` and `+376` appear only before
`0x82303FB0`. Everything after is the three axes. The differential records the
five axis fields and explicitly does not compare them.

## The differential

```
live_flight_ramps_microexec=pass cases=12 values_compared=60
```

Twelve cases: both lag directions, arrival, overshoot, command separation, both
secondary directions, both bounds, the gate reaching `+368` and not `+372`, the
bypass, and full-mantissa inputs.

The first run failed one case — `bypass` — with a decode fault at `PC=0`. The
bypass path dispatches slot 38 through the model's vtable, and my seed left
`[model+0]` null. Supplying a synthetic vtable with a **real** address at slot 38,
stubbed, fixed it; and the stubbed-call count is now asserted at one on the
bypass path and zero elsewhere, which is the evidence that bit 7 selects that
dispatch and nothing else does.

## What this does not close

The three axis blocks — `+304`, `+308`, `+312`, `+1352`, `+1356` — are 130
instructions of quadratic command shaping and per-axis lag, and they are the
next slice. Their inputs come from two 16-byte blocks at `[+576]` and `[+592]`
that nothing in this cycle read.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 9 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 19 behaviours
ctest                                 100% passed, 0 failed out of 38
tools/tests                           Ran 77 tests, OK
live_flight_ramps_microexec           pass 12 cases, 60 values
```

## Next

The three axis blocks of the same function, `0x82303FB0..0x823042B8`. They are
scalar, call nothing, and their footprint is already measured; the only new thing
is the two 16-byte input blocks at `[+576]` and `[+592]`, whose contents a
capsule can vary directly. The same audit extends by adding five columns to the
compared set rather than by being rewritten.
