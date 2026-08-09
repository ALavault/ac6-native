# Cycle 1383 — the models existed, and the boundary is real

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays 37. **No contract entry.**
- `analysis/microexec/calibration/live-rotation-blockers.tsv` **rewritten** — its
  previous version, committed yesterday, was founded on a false premise.

## Cycle 1382 was wrong, and in a way I should have caught

It concluded that four p-code operations "must be supplied" to run a rotation
live, costed them at 7,671 sites, and argued they were cheap because they are
pure data movement.

**They were already supplied.** `MicroExecuteFunction.java` has had models for
`vmrghw`, `vmrglw`, `lvlx` and `vrlimi128` since before this thread began,
registered by `registerAssertedSemantics` — gated behind the `vmx on` directive,
which is off by default *deliberately*, so that a snapshot without it contains
retail instructions only.

My spec never said `vmx on`. Adding that one line took the run from **601 steps
to 1034**, through both rotations, with no other change.

This is the **fourth** time this session a wrong conclusion came from not
checking what the repository already has — the thirty-first shape, which I wrote
myself three days ago. Twice it was a reimplemented tool; once a bound; now a
feature that exists and is switched off. The shape's rule says "if a repository
tool already answers the question, call it"; it evidently needs the weaker
companion **"and read the harness's own directive list before concluding it
cannot do something."**

## What actually remains

Eight operations, measured by running rather than by reading nine mnemonics:

| p-code op | sites | kind | assertable? |
|---|---:|---|---|
| `vmaddfp` | 1096 | arithmetic | yes, but plausible-wrong on error |
| `vnmsubfp` | 708 | arithmetic | same |
| `vcfsx` | 591 | convert | yes, exact |
| `vsel` | 514 | select | yes, pure movement |
| `vcmpeqfp` | 442 | compare | yes, exact |
| `vupkd3d128` | 264 | unpack | yes, pure movement |
| **`vrsqrtefp`** | 274 | **estimate** | **no** |
| **`vrefp`** | 244 | **estimate** | **no** |

`vrefp` and `vrsqrtefp` are specified only to a **relative accuracy bound**;
their exact bits are a property of the hardware. A model would return plausible
numbers that are not the console's, and every value downstream would be plausible
and wrong — the failure mode this whole campaign is built to avoid.

They are **refused, not deferred**. That is a different statement from "not yet
implemented" and belongs in the record as one.

## And the integrator always reaches them

Its normalise is guarded on all three scaled rates being below 2⁻¹⁶, so setting
the rate scale `[model+32]` to zero ought to skip it. It did not.

Control — two runs identical except for the bias registers:

```
f25 = f24 = 0                 60 steps, step_limit, no fault   normalise SKIPPED
f25 = 9.8/3.6, f24 = 1/pi     36 steps, fault at 0x823035F8    normalise ENTERED
```

The fused gravity bias at `0x82303584` makes the middle component non-zero on its
own, and `f25` is `9.8/3.6` whenever gravity is active — every frame.

So a live end-to-end differential of the integrator is **not reachable by this
instrument**, and the reason is a property of the hardware rather than a gap in
the tooling.

## What that does not invalidate, and the distinction is the point

The normalise block writes **only** the stack vector and then
`[model+128/132/136]`. It never touches `r30`, the position block. The three
position stores at `0x82303598`, `0x823035AC` and `0x823035B8` all **precede**
it, and the 10.0 floor that follows reads the position back from memory.

So the arithmetic `retail_flight_step` contracts is **identical on both paths**,
and cycle 1373's differential — which forces the skip by seeding `f11` — measures
a real branch of the function whose result the other branch shares. The
contracted behaviour stands, and now it stands for a stated reason instead of an
unexamined one.

What is genuinely out of reach is the **direction output** at
`[model+128/132/136]`, which is not ported and whose header already says so.

## Not established

- Whether the six assertable operations would unblock anything else worth having.
  The integrator is the reason they were wanted, and it is closed by the estimate
  boundary regardless.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 29 (1351–1371, 1374, 1376–1379, 1382, 1383) |
| implementation/integration spent on A3.2 | 8 (1354–1356, 1372, 1373, 1375, 1380, 1381) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 18 behaviours
ctest                                 100% passed, 0 failed out of 37
tools/tests                           Ran 77 tests, OK
```

## Next

A3.2's model side is closed: the step and all three virtuals it dispatches are
contracted, and the one thing that cannot be differentiated has been shown to be
unreachable for a hardware reason rather than a tooling one.

The plan's next named slice is **A3.3, the gameplay camera** — and it converges
with the same transform kernel, since `0x822A1E80` is where A3.1 derived the
rotation order in the first place. Before that, one bounded question is worth an
hour: **what steps the `+2224` object**, still open since cycle 1370. Nothing
forms `entity+2224` at runtime, so the flight model is reached from a list, and
the base constructor's call to `0x82282090` at `this+544` is the untested
candidate.
