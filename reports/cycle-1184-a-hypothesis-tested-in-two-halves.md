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

## Addendum — the better test was run, and it refutes the pairing

Cycle 1184 named the test it had not run: for each colour texture, whether some
*other* texture in the same pack is a plausible same-or-half-resolution partner
with markedly lower saturation. Thresholds fixed before looking at the outcome —
colour at saturation ≥ 6, grey at ≤ 3, partner at the same or half dimensions.

```
colour textures                                          50
  with a same-or-half-res grey partner in the same pack   36
  without                                                 14
observed match rate                                    72.0%

control: the same matching over shuffled packs, 200 trials
  mean 77.3%    max 92.0%
```

**The observed rate is below the null.** Assigning every texture to a random pack
produces *more* apparent diffuse/specular pairs than the real packing does. So
there is no pairing signal — not a weak one, none — and the 79% near-grey
population is not organised as partners of the coloured 11%.

The control is what makes this a refutation rather than a shrug: a 72% match rate
looks like strong support until the null says 77%. Most textures are grey, and
most packs are small, so a grey partner is nearly always available by accident.

## Where this leaves both readings

- **Mine (cycle 1170)**, that these are base-colour atlases tinted by the
  material: weakened by 79% near-grey, and not restored by any of this.
- **Diffuse/specular pairs**: the structural claim is now refuted with a control.
  What remains true is the observation that started it — four textures in five
  carry almost no colour, and something has to account for that.

Neither reading explains the population. That is the honest state, and it is a
better one than either side of the argument being allowed to stand.
