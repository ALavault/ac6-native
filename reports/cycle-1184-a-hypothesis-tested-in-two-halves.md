# Cycle 1184 — diffuse/specular: one half supported, one half not visible, one test flawed

## The hypothesis

Looking at the decoded world atlases, the observation was offered that they look
like **diffuse/specular map pairs**. It is a reasonable read — most of them are
strikingly desaturated — and it is testable, so it was tested rather than
accepted.

## Half of it is supported

Saturation, measured as mean `max(rgb) − min(rgb)` over 436 decoded world
textures:

```
min 0.0    median 2.1    max 18.9
near-grey (saturation < 4):  345 of 436   = 79%
```

Four textures in five carry almost no colour. That is what a specular, gloss or
mask map looks like and it is not what a diffuse atlas looks like. Cycle 1170
called them "mostly desaturated, which is what a base-colour atlas looks like
when tint arrives from the material" — that reading is now the weaker one, since
79% near-grey is a lot of atlases to explain by tinting.

## The pairing is not visible

If the packs held diffuse/specular pairs, the pairing should show:

```
packs where every size occurs an even number of times      2 of 82
consecutive-GIDX neighbours                                168
   of which share dimensions                                82   (49%)
```

49% is a coin flip. And the size distributions look like pyramids rather than
pairs — `e03_02` holds 2048×1, 1024×2, 512×2, 256×2; `e05_05` holds 128×1,
64×2, 32×4.

## The test is flawed, and saying so is the point

**A specular map is routinely authored at half the diffuse's resolution.** So
"sizes do not pair" does not refute the hypothesis. What it refutes is the
narrower claim that pairs are *same-size and adjacent in GIDX order* — which is
what I actually measured, and it is not what was proposed.

So the honest state is three-way:

- **supported**: most of this population is not colour data;
- **not visible**: any pairing structure, under the one test run;
- **not refuted**: the diffuse/specular reading itself, because the test that
  would refute it has to survive half-resolution seconds and non-adjacent ids.

A better test exists and was not run: for each texture, whether some *other*
texture in the same pack is a plausible half-or-equal-resolution partner with
markedly higher saturation. That is a matching problem rather than a counting
one, and it wants a fresh pass.

## Why record a half-answer

Because the alternative was to report the 79% and let it stand as agreement. The
number supports the observation; the structure I checked does not show it; and my
check could not have shown it even if it were true. Reporting only the first of
those three would have been the most flattering and least useful thing to say.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

No product code changed. Measurement is over locally decoded pixels; none is
committed.
