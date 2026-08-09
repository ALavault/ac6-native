# Cycle 1438 — it was never terrain

## Qualification

- **No Ghidra run and no oracle pass.** The package and the product's ports.
- No product C++ changed; ctest stays **53**. **No contract entry.**
- New capture `ndxr-aigaion`; the roster README corrected.

## One constant, and the question answered

The renderer was fixed at 480×270. Cycle 1437 named that as the limiting
instrument and it was: at 1600×900 the same mesh, unchanged, is unmistakably a
**flying wing with eight engine nacelles and a central hull**, 2409 units across.

The record is `o_1112_lod1_O_OBJ_O_HIR`. Ace Combat 6's aerial fortress is the
**P-1112 Aigaion**, and it leads the attack on Gracemeria in Mission 01.

The identification is a reading and is labelled as one — `o_1112_lod1` and
`2409 × 170 × 1082` are the file's; "the Aigaion" is mine. What supports it: the
number in the name is the designation, the scale matches, and nothing else in
the game is a 2.4-kilometre flying wing.

## What I had wrong, for three cycles

Cycles 1428, 1429 and 1436 all call entry 2 "the terrain". **On nothing but its
size** — 2409 units against a 15-metre tank — and the roster README carried it
in a table.

It is not terrain. **Mission 01's package has no terrain model in the roster at
all**, which is a more useful thing to know than the label it replaces, and it
sat behind a guess applied once and never rechecked.

Cycle 1428's own report says the labels in that table are mine and the numbers
are the file's. It says so and I then leaned on one of the labels for three
cycles as though it were a number.

## Why it lasted

Every cycle after 1428 had a reason not to look: 1429 was about placement, 1436
about texturing, 1437 about a transform hypothesis. Each treated "the terrain"
as settled context and spent its attention elsewhere.

**A label that is never the subject of a cycle is never checked by one.** The
thing that killed it was not a new technique but a bigger window — and the
window had been one constant away the whole time.

## Not established

- What `nmbs001..368` and `hire01..` are. They are the bundle's other records,
  368 and 196 of them, all sharing one local space.
- Whether Mission 01 has a terrain model elsewhere — in another MDLP entry, in
  another archive index, or not as an NDXR at all.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=4
```

## Next

**Find the ground, or establish there is none here.** The roster is 38 models and
none of them is terrain: the largest is the Aigaion, then two ~1480-unit records
(`mapobj_m01_l_brg1` and `brg2` — bridges, by their names), then buildings at
~50 units.

`mapobj_m01_` is the prefix the plan named for Mission 01's map objects, and two
of them are in the roster while the rest are not. That is where to look, and it
is a search of the package by name rather than by size — which is what should
have identified entry 2 in the first place.
