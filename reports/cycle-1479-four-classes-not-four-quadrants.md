# Cycle 1479 — four classes, not four quadrants

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ unchanged; ctest stays **59**. **No contract entry yet** — the
  correction below changes a contracted statement and is carried into the
  contract in the next cycle, not asserted here.

## The reviewer's observation

> "Les reconstructions du pont semblent avoir le même problème de LoD que les
> avions."

Cycle 1430 found exactly that for aircraft: a model carries `_lod1`.._lod4` and
`_crash1..4` as separate **records**, and drawing them all superimposes every
level of detail and every wreck. `tools/ndxr_model_textured.cpp` has carried a
name filter for it ever since.

`tools/mission01_scene_render.cpp` has **no filter at all**. It draws every
record of a model at every instance of that model.

## And the counts say more than that

The map package's record names carry a class token after `_m01_`:

```
l=289   m=584   s=3277   x=112   airport=56      total 4318
```

The placement list holds **4,318 instances**. The package holds **4,318
records**. And the tag's bits 30..31, which cycle 1452 read as a two-bit
rotation quadrant:

| | records | | tag bits 30..31 |
|---|---:|---|---:|
| `l` + `airport` | **345** | 0 | **345** |
| `m` | **584** | 1 | **584** |
| `s` | **3277** | 2 | **3277** |
| `x` | **112** | 3 | **112** |

Four for four, to the unit — and `x` minus the **92** records `0x82102350`
skips is **20**, which is exactly the accepted count in class 3.

> **Bits 30..31 are the draw-distance class**, the same `l`/`m`/`s` the mapset
> names in `.mapparts.distanceL/M/S` = 16000 / 12000 / 10000.

## Which corrects two cycles

- **Cycle 1452** read bits 30..31 as a two-bit rotation quadrant. They are the
  class.
- **Cycle 1451** measured a four-fold structure and called it a street grid at
  79.33 degrees. The four-fold structure is **four classes**. That cycle's
  statistic was real, its mechanism was wrong, 1452 corrected the mechanism to
  "a two-bit field" and got the *meaning* wrong in turn. This is the third
  reading of the same four values and the first with an exact external join.

The contract's `retail_map_placement` statement says bits 30..31 are "a two-bit
field taking all four values", which remains true and is now under-specific; the
next cycle carries the class reading into it with these counts as the evidence.

## The render

Drawing only the records whose class token matches the instance's own class:
`mission01-scene-class-filtered.png`. The bridge is one bridge — towers, main
cables, hangers — instead of every variant of itself stacked.

## The second observation, and where it goes

> "Il manque tous les effets de lumières/shader."

Also true, and the material is already in hand rather than hypothetical. The
mapset XML cycle 1474 opened carries, unused: `HDR` (10 values), `LensFlare`
(40), `Vignetting` (3), `LevelCorrection` (15), `Saturation` (2), and
`.sky1.Weather.LightShaft.*` (Ratio, Colour RGBA, UpperWidth, LowerWidth, Length,
FadeNear 16000, FadeFar 24000). The renderer applies one lambert term and
nothing else.

A parallel sweep is reading those groups and the material parameter chain now;
this cycle does not pre-empt it.

## Not established

- Which record a `x`-class instance draws when a model holds both `l` and `x`.
  Model **163**, the bridge, is the only one of 170 carrying two class tokens
  (`l`=2, `x`=20), so the rule is chosen on one example.
- Whether the class also selects a *mesh* level of detail rather than only a
  draw distance.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
tools/tests                             Ran 79 tests, OK
```

## Next

**Carry the class reading into the contract**, with the four-way join as its
evidence, and correct `retail_map_placement.h`'s account of bits 30..31 by name
and cycle number. It is the third reading of that field and the first that an
independent count agrees with.
