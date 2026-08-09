# Cycle 1396 — the camera is blocked, and the rest is not

## Qualification

- **No Ghidra run and no oracle pass.** The corpus and the CALLOTHER census.
- No product C++ changed; ctest stays 45. **No contract entry.**
- New artefact `analysis/flight/camera-estimate-census.tsv`.

## The cheap check, run before reading

Cycle 1395 ended on a warning rather than a plan: `0x82300C20` has 422 vector
instructions, and if the camera uses the estimate instructions that put the
flight model's position out of reach, a faithful camera is out of reach too. One
count settles it, and it costs nothing.

| function | insns | vector | **estimates** | verdict |
|---|---:|---:|---:|---|
| **`0x82300C20`** | 1985 | 422 | **14** | **blocked** — `vrefp`×6, `vrsqrtefp`×8 |
| `0x8209B398` | 400 | 14 | 0 | reachable |
| `0x820938B8` | 50 | 14 | 0 | reachable — the constructor |
| `0x82212BD8` | 152 | 0 | 0 | reachable |
| seven others | ≤137 | 0 | 0 | reachable |

**Ten of eleven are reachable, and the one that is not is the one that matters.**

## Which it is, established rather than guessed

Cycle 1395 named `0x82300C20` a candidate "on position rather than on name" and
refused to go further. Its **only** caller is `0x82234040` — 742 instructions,
which reads `entity+4912`, the live flight model pointer, **seventeen times**,
more than any other function in the image (cycle 1371's scan).

So `0x82300C20` is the camera work of the frame that flies the aeroplane. That is
the gameplay camera, and now it is a derivation and not a neighbourhood.

## The decision this records

**A bit-exact gameplay camera is not reachable by micro-execution**, for exactly
the reason the position integrator is not: `vrefp` and `vrsqrtefp` are specified
only to a relative accuracy bound, their exact bits belong to the console, and
cycle 1383 refused to model them.

So **a demo uses a camera of its own, captioned as such** — the same statement
the flight session already makes about producing attitude and not position.

The alternative is worth naming so it can be rejected explicitly: model two
estimate instructions, port `0x82300C20` on top of them, and call the result
retail's camera. That would put plausible numbers where measured ones belong, in
the one part of the product a viewer looks at *directly* and cannot check. Every
other approximation this campaign has refused was invisible; this one would be
the first that is not, which makes it more tempting and worse.

## What that leaves

Two questions, and they are different:

- **For fidelity**: the ten reachable functions. `0x8209B398` (400 instructions,
  14 vector, no estimates) and `0x82212BD8` (152, scalar) are the largest, both
  unread and both bounded. They are camera *data* work — building or updating a
  `CGaCamera` — and whatever they compute is portable and differentiable.
- **For the demo**: nothing. A camera that follows the contracted attitude is
  twenty lines and needs no retail function at all.

Those should not be confused, and the plan's "A3.3, the gameplay camera" was one
item that is now two.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 2 (1395, 1396) |
| implementation/integration spent on A3.3 | 0 |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 45
tools/tests                           Ran 77 tests, OK
```

## Next

The demo, because it is now unblocked and small: a camera of my own following the
contracted attitude, the flight session driven by a scripted or live stick, and
the existing raster target. Every piece exists; none of it waits on a decision.
The caption is the deliverable as much as the picture — it must say that the
attitude is retail's, measured, and that the camera and the scene are not.

Then `0x8209B398` for fidelity, on its own schedule.
