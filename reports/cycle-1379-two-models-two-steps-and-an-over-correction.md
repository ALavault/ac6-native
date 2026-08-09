# Cycle 1379 — two models, two steps, and an over-correction

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus and the image.
- No product C++ changed; ctest stays 35. **No contract entry.**
- New artefact `analysis/flight/two-steps-two-models.tsv`; new shape 35 in
  `INSTRUMENT_DISCIPLINE.md`.

## The worry from cycle 1378 is answered

`+4912` — the pointer about a hundred sites read for flags, speeds and limits —
is written **exactly twice in the whole corpus**, both in constructors
(`0x8222BFB0`, `0x82293C6C`). Nothing repoints it later. It points at the
`0x8200F310` branch and never at `0x8200F270`, the class whose integrator and
control surfaces are contracted.

**Being pointed at by `+4912` is not what makes a model fly. Having a non-empty
step is.**

| vtable | slots | slot 11 | slot 15 |
|---|---:|---|---|
| base `0x82008B10` | 36 | `0x82283898` | empty |
| `0x8200F270` | 36 | `0x82283898` inherited | empty |
| `0x8200F310` | **40** | **empty** | `0x82306A38` |
| `0x8200FC28` | **40** | **empty** | `0x82329968` |

`0x8200F270` is stepped by the base's slot 11, which calls its slots 30, 31 and
32 — the contracted control surfaces, the contracted integrator, and the
orientation update. **The class this campaign ported is genuinely a class that
flies.**

The `0x8200F310` branch overrides slot 11 with the empty `blr` and is stepped by
`0x82306A38` at slot 15, with the *same* signature `(this, step, position block)`
and calling the same slots 30/31/32, plus `0x82282938` and `0x82326FE8`, which
the base step also calls.

Two flight models, two drivers, one entity.

## I over-corrected cycle 1377, and that is the shape

Cycle 1377: *"the handling is a performance curve resampled by speed, not a
static set of numbers."*

Cycle 1378: *"that is true of the sibling, and it happens on reset, because
slot 29 is a reset."*

Slot 29 *is* a reset. The correction was still wrong. `0x82306A38` — the
sibling's own per-frame step — calls the lookup `0x82283480` at `0x82306BDC`
**every frame**. Cycle 1377 was right about the class it was describing; cycle
1378 replaced a true statement with a false one while sounding more careful.

The failure was concrete and cheap to have avoided: `0x82283480` has **three**
callers, and 1378 read one, then stopped because the answer was satisfying. A
first claim with one supporting site would have been challenged. The same claim
wearing the clothes of a correction was not.

That is the **thirty-fifth shape**, indexed: *a correction is a new claim and
takes the same evidence as any other, and receives almost none.*

## And the vtables are not all 36 slots

Cycles 1371 and 1375 both said "36 slots". Measured on **each vtable's own
words**, base and `0x8200F270` stop at 36; `0x8200F310` and `0x8200FC28` run to
**40**. Slot 39 at offset 156 is dispatched by `0x82306AD8`, and it is a code
pointer only on those two.

The error was applying one stopping rule across vtables of different lengths —
the twenty-fifth shape in a new form. Nothing derived from it was wrong, because
every slot used so far is below 36, but the number was stated and was false.

## Not established

- What steps the `+2224` object. Nothing forms `entity+2224` at runtime
  (cycle 1370) and slot-11 dispatch objects come from `+4`, `+0`, `+16` and a
  dozen other offsets — a list walk. The base constructor's call to
  `0x82282090` at `this+544` is the untested candidate for a registration.
- Slots 36–39 of the `0x8200F310` branch.
- What `0x82282408` copies from.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 27 (1351–1371, 1374, 1376–1379) |
| implementation/integration spent on A3.2 | 6 (1354–1356, 1372, 1373, 1375) |

Five research cycles since the last code. The next slice ends with a behaviour:
`0x82302C88` is the remaining unported slot of a class that is now *established*
to fly, its two math seams are certified at 0 ulp, and its three rotations are
already in the product.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 16 behaviours
ctest                                 100% passed, 0 failed out of 35
tools/tests                           Ran 77 tests, OK
instrument_discipline_index           pass shapes=26 unindexed=0
```

## Next

Port `0x82302C88`, the orientation update — the third and last pure virtual of
the contracted class. Everything it needs exists: `rotate_820A9B30`,
`rotate_820A99F8` and `rotate_82211828` are in `retail_transform.cpp`; `asin` and
`atan2` are measured identical to libm; the angle formulas and the per-axis
limits are read. The differential is the open question — two of its six callees
are VMX128 — and the answer is probably to port and test the **angle
computation** as its own behaviour, which is pure scalar and needs no bridge.
