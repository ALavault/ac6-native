# Cycle 1481 — the post-process

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ **changed**: `apply_mapset_post` added with its test.
  ctest **59 → 60**.
- **No contract entry.** The values are retail's; two interpretations and all
  three curves are mine, and a contract entry would claim otherwise.

## The reviewer's second observation

> "Il manque tous les effets de lumières/shader."

True, and the numbers were already extracted. `022_FHM`'s XML carries a complete
post-process description:

```
.LevelCorrection.In.Min      13 /  7 / 13        .Out.Min   5 / 5 / 5
.LevelCorrection.In.Gamma   0.9 / 1.0 / 1.1      .Out.Max 235 / 235 / 235
.LevelCorrection.In.Max     229 / 245 / 255
.Vignetting.bEnable 1   .fRadiusRatio 0.610   .fFovRatio 1.5
.HDR.bEnable 1  .fBrightPassThreshold 0.4  .fBloomSigma 2.0  .fBloomScale 1
.Saturation.Enable 0    .Delta 0
```

**`.Saturation.Enable` is 0**, so nothing here touches saturation. Honouring a
disabled effect is as much a reading as applying an enabled one, and it is the
easy one to get wrong: a renderer that "adds a bit of saturation because it looks
better" has stopped reproducing this map.

## What is retail's and what is mine

Retail's: every value above, and the decision to apply levels, a vignette and a
bloom at all.

Mine: the **curves**. The file gives a gamma but not whether it is `x^g` or
`x^(1/g)`; this uses the Photoshop-levels convention `x^(1/g)` and says so in the
header. The vignette's falloff shape and the bloom's kernel are mine too. So two
interpretations and three shapes are inventions sitting on twenty retail numbers,
and the header lists which is which.

## The control

The test checks the arithmetic where it is arithmetic, not the picture:

- the level curve reproduced channel by channel from the file's own numbers;
- **a grey input does not stay grey** — with gammas 0.9 / 1.0 / 1.1 the channels
  must diverge, and asserting that is what makes "per-channel" more than a word;
- black floors at `Out.Min` = 5 and white ceils at `Out.Max` = 235;
- the vignette leaves the centre exactly untouched and everything inside
  `fRadiusRatio` untouched, which is what that number means;
- a frame below the bright-pass threshold is **bit-identical** after bloom.

That last one is the useful negative: a bloom that lifts everything is a bloom
with the threshold ignored.

## The render

`mission01-scene-postprocess.png`. The horizon carries the bloom's haze, the
corners fall off, and the tone has the file's per-channel shift.

## Not established

- Whether retail applies levels before or after the bloom. This blooms first;
  the file states no order.
- `.HDR.fMinLuminance` / `fMaxLuminance` / `fAdaptationSpeed` — an eye
  adaptation this does not implement, and `.fBrightPassOffset`, `.fStarScale`,
  `.fBloomBrightness`, which are read and unused.
- `.Vignetting.fFovRatio` = 1.5, read and unused.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**The parallel sweep, which is still running.** Six investigations over the
material parameter chain, the terrain texture, the `.sph` sky, the trees, the
unexplained containers and the mapset's remaining groups. Two cycles have now
been chosen around it rather than from it; the next should be from it.
