# Cycle 1169 — the byte swap stops being visual, and the corpus stops me overclaiming

## The last visual dependency in the texture domain

Everything about NTXR is now derived or measured except the 8-in-16 byte swap,
whose only control on record is `NTXR_STRUCTURE_REPORT.md`'s: omitting it
"produces visibly corrupted colors". That is a negative visual control, which is
better than nothing and worse than everything else in the file.

There is a non-visual measure available. A natural image is smooth; a decode
through the wrong endianness corrupts every block's RGB565 endpoints and is not.
So: decode every wrapper **both ways** and score each by the mean absolute
difference between horizontally adjacent texels.

## The result, on two populations

```
436 world textures inside the MDLP
  mean total variation, swap applied      7.17
  mean total variation, not applied      26.84
  smoother with swap 424   without 0   tied 12

668 UI wrappers, the FHM children
  mean total variation, swap applied     11.89
  mean total variation, not applied      19.54
  smoother with swap 468   without 170  tied 30
```

On the world textures it is unanimous — not one of 436 is smoother without the
swap, and the mean is 3.7× worse. That is a falsification test the decode passed
with no exceptions.

## Where the corpus stopped me

I wrote the assertion first and ran it second, and it failed: `plain_smoother
== 0` is false on the UI corpus, where 170 of 668 wrappers score better without.

That is not a defect in the measure. Fonts and HUD panels are largely flat black
with hard edges, so both decodes score alike and noise picks a winner. The
measure discriminates on the population it is meant to — images — and shrugs on
the population that is mostly not images.

The assertion now says what this corpus shows: the aggregate variation is lower,
the majority is better than two to one, and the exact split 468/170/30 is pinned.
Asserting unanimity here would have imported a result from a population the test
does not read — which is the same error as citing a measurement from one corpus
to license a claim about another, and I was one `ctest` run from committing it.

## What this does and does not change

The swap is **still not derived**. No instruction has been read that performs it,
and it remains an explicit argument to `decode_ntxr_base_level` rather than a
constant, because a measured control over one corpus is not the same as reading
the code. The header now states the numbers instead of citing the visual note.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```
