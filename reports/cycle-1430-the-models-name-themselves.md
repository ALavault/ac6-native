# Cycle 1430 — the models name themselves

## Qualification

- **No Ghidra run and no oracle pass.** The product's ports over the package.
- No product C++ changed; ctest stays **53**. **No contract entry.**
- New: `tools/ndxr_model_names.cpp`,
  `analysis/assets/mission01-model-names.txt`. All three NDXR captures
  regenerated.

## The cycle went somewhere better than planned

The plan was to read the per-unit heading. It is still unread — `ScenarioObjScalars`
holds three floats and they are the position, so a heading is elsewhere in the
Obj block and this cycle did not find it.

What it found instead answers a question three cycles have been guessing at.
**`NdxrRecord` has carried a `name` since it was ported** — `rec+0x20`, resolved
against the string table — and nothing had read it.

```
id   6  used  18  : o_c17g_lod1 o_c17g_lod2 o_c17g_lod3 o_c17g_lod4
id  20  used  10  : o_b52h_lod1 o_b52h_lod2 o_b52h_lod3 o_b52h_lod4
id  16  used  28  : o_f16c_lod1 ...        id  24  used  24  : o_f18f_lod1 ...
id  28  used   1  : o_f22a_lod1 ...        id  72  used   8  : o_su33_lod1 ...
id  14  used  34  : o_mr2k_lod1 ...        id  26  used  16  : o_rflm_lod1 ...
id   0  used   1  : o_e767_lod1 ...        id  45  used   3  : o_ah64_lod1 ...
id   4  used  11  : c_ftnk ...             id   8  used  16  : c_apcr ...
id  10  used  16  : c_pcht ...             id  38  used  29  : c_saag ...
id  48  used   1  : s_aegs ...             id  64  used   4  : s_dstr ...
id  18  used   1  : mapobj_m01_l_brg1 ...  id  19  used   1  : mapobj_m01_l_brg2
```

Thirty-eight names, in `analysis/assets/mission01-model-names.txt`.

## And it settles an identification I had made by eye

Cycle 1429 called id 6 "a four-engine heavy transport". It is **`o_c17g`** — and
its measured extent, `53.02 × 51.50 × 17.03`, is the real C-17's 53.0 × 51.75 ×
16.8 to under half a percent.

That agreement is worth more than the name. **It is an independent check that
the units are metres and the decoder's absolute scale is right** — nothing in
this campaign chose the scale, and a model that matches a real aircraft's
dimensions to 0.5% could not have come out of a wrong stride or a wrong base.

There is also a B-52 in the mission — **id 20, `o_b52h`, used ten times** — and
it is a different model with eight engines in four twin pods. The wireframes are
distinguishable and now so are the labels.

## A defect in my own three captures

Every model carries `_lod1` through `_lod4`, and most carry `_crash1`..`_crash4`
— destroyed states. **All of them were being drawn superimposed.**

The C-17's four levels are 1547, 629, 166 and 6 vertices, and 1547 + 629 + 166 +
6 is exactly the 2348 cycle 1429 reported. The roster's 154,351 vertices become
**116,936** once the wrecks and lower levels are dropped.

All three captures — `ndxr-model-04`, `ndxr-mission-roster`,
`ndxr-mission-placed` — are regenerated at `lod1`, with metrics refreshed and
the image checker green at 9 of 9.

**Selecting `lod1` is a choice and is declared as one.** Retail picks a level by
distance; that rule is not read here.

## What this says about reading a header before using it

`NdxrRecord::name` was ported, documented, and sitting in the struct while
cycles 1427, 1428 and 1429 identified models by looking at wireframes and
guessing. The information was one field away the whole time.

That is the thirty-first shape again — the repository already answers it — but
in a form worth separating: not an unused *tool*, an unused *field on a struct I
was already reading*. `Record()` was being called for its `descriptor_count` on
every one of those cycles.

**When a struct answers more than the question you asked it, read the rest of
it.**

## Not established

- The per-unit heading, which is what this cycle set out to find.
- What `crash1..4` are geometrically — four progressive wreck states, or four
  pieces of one. They are dropped, not read.
- The `auto_mot_a`, `prog_mot_a`, `nigt01`, `canp`, `swp1` records that appear
  beside the LODs. They are kept when a model has no `_lod` name at all, which
  is a rule that fits the data and was not derived.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=9 (all three captures)
```

## Next

**The heading, still.** With the names in hand the scene is worth fixing: 18
C-17s and 10 B-52s all facing the same way is the one obviously wrong thing left
in the picture, and the scenario carries the answer beside a position this
product already reads.

Then the decoder's contract entry, which has now waited three cycles.
