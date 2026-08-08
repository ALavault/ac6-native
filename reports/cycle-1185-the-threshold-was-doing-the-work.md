# Cycle 1185 — the decoder is exonerated, and my threshold was doing the work

## The explanation that had to be ruled out first

"79% of the world textures are near-grey" has an obvious alternative cause: a
colour bug in my decoder. It would produce exactly that symptom, and it would
make every conclusion downstream worthless.

The UI corpus is the control — same decoder, same code path, different data:

```
UI corpus (FHM children)      n=334  median  7.3  max 203.1  near-grey 35%
world records (MDLP packs)    n=436  median  2.1  max  18.9  near-grey 79%
world pack headers            n= 86  median  2.4  max  18.3  near-grey 68%
the 4 single-GIDX facades     n=  4  median  9.5  max  13.6  near-grey  0%
```

**The decoder produces saturation up to 203.** It is not dropping colour. The
world textures are muted in the data.

## But the fourth row is the interesting one

The four facade textures — the ones cycle 1162 showed and I described as concrete,
brickwork and guard rails, plainly coloured to the eye — score a median of **9.5**
and a maximum of **13.6**. They sit inside the same 0–19 band as everything else
in the world population, and 0% of them are "near-grey" only because the
threshold is 4.

So the band is not evidence of non-colour data. It is what muted material looks
like on this scale: concrete, metal and camouflage occupy roughly 2–14, while the
UI reaches 203 because it contains saturated primaries — the RGB test pattern
alone will do that.

**My "79% near-grey" was a threshold artefact.** A cutoff of 4 on a population
whose *visibly coloured* members score 9.5 is not measuring "carries no colour",
it is measuring "is less saturated than a menu". Cycle 1184 leaned on that number
to weaken my own cycle-1170 reading; that weakening was unearned.

## Where the argument actually stands

- **The pairing refutation stands.** It rested on a shuffled-pack control, not on
  the threshold, and the observed match rate was below the null.
- **The "79% carry almost no colour" claim is withdrawn.** The number is real and
  the interpretation was wrong.
- **Cycle 1170's reading is restored to where it was**: these look like base
  material atlases, muted, which is unremarkable for aircraft and concrete.

Nothing about the decoder or the extraction changes. What changes is that a
comparison across corpora was available the whole time and I reached for a
threshold instead.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

No product code changed.
