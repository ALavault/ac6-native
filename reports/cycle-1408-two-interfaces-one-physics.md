# Cycle 1408 — two interfaces, one physics

## Qualification

- **No Ghidra run and no oracle pass.** This cycle ports no retail function.
- **Product C++ changed**: `demo_flight_view.cpp` (the caption), its tests.
  ctest stays 49.
- Capture `reports/mission01-native-captures/demo-flight-model/` regenerated
  through the contracted player path; its README and CAPTION rewritten.
- **No contract entry.**

## The capture now runs on the contracted path

The emitter drove `FlightStick` — the AI's setter interface — because when it was
written that was the only contracted way in. It now builds a **controller record**
each frame and drives the player path: the contracted binding layer,
`0x82227E10`'s five increments, the five clamped accumulators.

The stick positions in the loop are the only thing it still chooses.

## The frames are byte-identical, and that is a result

Nine frames, nine identical SHA-256s to the previous version. My first reading was
that the emit had not taken.

It had. The two paths genuinely agree here:

- the old capture drove the setter interface with an **increment of 1.0**;
- this one drives the accumulators with a **binding output of 0.72** —
  `0.8` less an `0.08` deadzone;
- both **saturate the accumulator's clamp within two frames**, and the frames are
  sampled every 200, so from the first sample onward the attitude is the same;
- at frame 0 the difference is a rotation of about **2.8 × 10⁻⁵ radians** —
  sub-pixel at 480×270.

**Two different retail interfaces, one physics.** That is what the accumulators
being shared state means, and it is a stronger statement about the rewire than a
picture that changed would have been: nothing downstream noticed, because nothing
downstream was touched.

Worth naming the near-miss: identical output after a change is exactly the
signature of a change that did not take. The check was arithmetic — what does
each path put in the accumulator, and when does it clamp — not a re-run.

## The caption no longer claims an invented conversion

It used to say the stick-to-command conversion was mine. Cycles 1405–1407
contracted retail's own and deleted the invention, so that clause is gone. What
the caption now lists as invented is the camera, the scene, the axis assignment,
and **which controller axis feeds which input field**.

Two tests hold it there: one asserts the caption mentions the wiring, and one
asserts it **no longer contains the words "full-scale"** — the phrase the deleted
invention used. A caption that can drift back is worse than none.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 8 (1395, 1396, 1399–1404) |
| implementation/integration spent on A3.3 | 6 (1397, 1398, 1405–1408) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 27 behaviours
ctest                                 100% passed, 0 failed out of 49
tools/tests                           Ran 77 tests, OK
```

## Next

`0x82229250`'s other four writes — what fills `+2096`, `+2100`, `+2112` and
`+2116`. Two of the six input fields are traced to the binding layer; tracing the
other four would remove "which controller axis feeds which input field" from the
caption's invented list, leaving only the camera, the scene and the axis
assignment.
