# Cycle 1395 — the camera is named, and it is not virtual

## Qualification

- **No Ghidra run and no oracle pass.** The class map, the image, the corpus.
- No product C++ changed; ctest stays 45. **No contract entry** — this opens
  A3.3 by bounding it.
- New artefact `analysis/flight/camera-subsystem-map.tsv`.

## Two structural facts, and both change the method

**It is named.** Every class in the flight thread was among the 306 vtables
without RTTI; cycle 1385 called finding *one* named class the thread's first
anchor. The camera subsystem is named throughout — `galib::CGaCamera`,
`ACE6::CAce6CameraManager`, `ACE6::CAce6ArmsCamera`, `CBriefingCamera`,
`CDebriefingCamera`, `CSelectAircraftCamera`, `CDemoMoveCamera`,
`CX360CameraManagerGT`, and eleven `CX360ReplayCameraGeneralTuner*` variants.

**It is not virtual-dispatch driven.** Measured: each of the three core vtables
holds **exactly one slot**, and that slot is a destructor.

```
galib::CGaCamera            0x82054DA8   1 slot   0x82093A10, 21 insns
ACE6::CAce6CameraManager    0x82054F0C   1 slot   0x82094BE8, 32 insns
ACE6::CAce6ArmsCamera       0x820552AC   1 slot   0x82094518, 18 insns
```

So these are **data classes**, and the per-frame camera work is in plain
functions that operate on them.

That is the exact inverse of the flight model, where every behaviour was a vtable
slot of an unnamed class and the entire difficulty was working out which slot
ran. A3.3 needs a different method, and knowing that on day one is worth more
than any single function would have been.

## And my own extent tool was wrong first

The first measurement said "1 slot" for all four vtables *including*
`galib::CGaLocator`, which I did not believe. Dumping the raw words showed why:
`CGaLocator`'s vtable really is one slot, and `CGaCamera`'s begins eight bytes
later behind its own COL. The heuristic was right and my disbelief was the error
— the twenty-fifth shape run in reverse, where the instinct that a vtable "must"
be longer nearly produced a correction to a correct reading.

The check that settled it was dumping `0x82054D88..0x82054DD0` and resolving
every word: seven consecutive `COL, slot` pairs, one per class, packed. That is
the MSVC layout `whose_vtable.py`'s docstring describes, seen directly.

## The handle A3.3 gets instead

Who **builds** a `CGaCamera`. The vtable is materialised at **29 sites in 27
distinct functions**, all sized and with their caller counts in the artefact.
`0x820938B8` has seventeen callers and is the constructor.

Two candidates for the gameplay camera, **on position rather than on name**:

- **`0x82300C20`** — 1,985 instructions, **422 of them vector**, one caller, and
  it sits at `0x8230xxxx`, the same span as the flight model's slot 30
  (`0x82303E68`) and its step (`0x82306A38`).
- `0x82212868` / `0x82212BD8` — small, at `0x8221xxxx`, where the contracted
  input consumer `0x82211C10` lives.

**Neither is established.** Position in the image is not evidence — that is the
refuted-link shape, and cycle 1379 was caught by a near relative of it. What *is*
established is the population, sized, with a named class to anchor it.

## What this means for the demo

The camera's 422 vector instructions are a warning: if the gameplay camera
depends on the same estimate instructions that put the flight model's position
out of reach (cycle 1383), then a *faithful* camera is out of reach too, and a
demo would have to use a camera of my own — captioned as such.

That is worth knowing before any of it is ported, and it is checkable cheaply:
count `vrefp` and `vrsqrtefp` in the candidates before reading them.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 1 (1395) |
| implementation/integration spent on A3.3 | 0 |

A3.2 closed at 32 research and 16 implementation cycles, with 25 contracted
behaviours and a chain that runs.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 45
tools/tests                           Ran 77 tests, OK
```

## Next

Count the estimate instructions in `0x82300C20` before reading a line of it. If
`vrefp` and `vrsqrtefp` are there, the honest plan is a camera of my own for the
demo and a contracted one only if the boundary moves; if they are not, the
candidate is portable and the next cycles look like A3.2's good ones — bound the
footprint by capsule, resolve the constants, then read.
