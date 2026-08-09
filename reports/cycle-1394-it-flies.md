# Cycle 1394 — it flies

## Qualification

- **No Ghidra run and no oracle pass.** This cycle ports no retail function.
- **Product C++ changed**: `retail_flight_session.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 45**, was 44.
- New artefact `analysis/flight/flight-session-trajectory.tsv`.
- **No contract entry, and it would be wrong to add one** — see below.

## The wiring

Twenty-five contracted behaviours, each verified against the retail instructions
that produce it, and until now none of them called another. This composes them in
the order `0x82306A38` calls them:

```
1. the three command setters, slots 12/13/14      retail_flight_command
2. slot 30: the ramps, then the axes              retail_live_flight_ramps / _axes
3. the rate servo, a direct call                  retail_flight_rate_servo
4. the decays                                     retail_live_flight_step
5. the rotation angles, applied in A3.1's order   retail_flight_orientation / _transform
6. the accessors and the export block             retail_control_blend / _flight_export
```

Hand it a stick position per frame and read back the aeroplane's attitude.

## Why it does not enter the contract

**It ports nothing**, so there is no retail function to differentiate it against,
and the campaign's rule is that a behaviour without a `microexec` does not enter.
Its correctness is entirely the correctness of its pieces — each of which has a
differential — plus the **order**, which is taken from the contracted
`retail_live_flight_step` and is the only thing this file asserts on its own.

Adding a contract entry for a composition would make the gate say something the
gate cannot check. The twenty-five stand; this makes them run together.

## What it produces

`analysis/flight/flight-session-trajectory.tsv` — a scripted three-second
manoeuvre, 180 frames at 1/60 s, one row each: the three command accumulators,
the three control axes, a rate, and the nine basis components.

```
frame  cmd36 …  at304        row1y            row1z
0      1        0.133333355  1                2.77036452e-05
61     1        1            0.999930084      0.011922…
123    1        1            0.997692466      0.0247696526
```

The basis rotates in response to the stick, at a rate the contracted chain sets,
and the run ends pinned to `digest 0xA975B7EB0ADFBCF2`. **That is an aeroplane
responding to a control input, computed entirely by rules taken from the retail
instructions.**

## What is absent, and it is not small

**Position.** The live model's position step is `0x823042D0`, and cycle 1383
established it is unreachable by this instrument for a **hardware** reason: its
normalise depends on `vrefp` and `vrsqrtefp`, estimate instructions whose exact
bits are a property of the console and which this campaign refuses to model
rather than approximate.

So the aeroplane **changes attitude; it does not move**. The header says so, the
artefact's own preamble says so, and any demo built on this must say the same.
It would be very easy to add a plausible integrator and call the result a flight;
that would be the first invented gameplay rule in 1,394 cycles.

Also chosen rather than derived: the aircraft's gains. The limits are the base
constructor's defaults (5.0, 1.4, 5.4, read at cycle 1377) but the servo gains
and lag coefficients are made up, because the fields that hold them
(`[+544..+568]`, `[+576]`, `[+592]`) are filled by `0x82282408` from a source
cycle 1384 could not reach. The artefact's preamble names that too.

## What a demo needs now

1. **a camera** — A3.3, unstarted, and it converges on the transform kernel that
   is already contracted;
2. **a renderer target** — the Vulkan surface and capture tooling exist;
3. **real aircraft numbers**, if the demo is to fly a *particular* aeroplane
   rather than a plausible one.

None of these needs a decision anyone has deferred. The one that does — JV, the
asset-loading arbitration — is only needed for a demo that shows *Mission 01*
rather than *the flight model*.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 16 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387–1394) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 45
tools/tests                           Ran 77 tests, OK
```

## Next

**A3.3, the camera.** It is the last thing between this trajectory and a picture,
it converges on `retail_transform` which is already contracted, and the plan
named it as converging with A3 from the start. After that, a demo of the flight
model — captioned for what it is.
