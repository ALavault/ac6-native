# Cycle 1451 — the fourth harmonic

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the ported decoders.
- Product C++ **changed**: `MapPlacement::four_fold_resultant` added, and the
  placement test extended. ctest stays **56**.
- Contract: `retail_map_placement`'s statement and native-test claim updated;
  still **34 behaviours**.

## The measurement

`tag >> 16` has been decoded and deliberately unnamed since cycle 1447. Reading
the u16 as a fraction of a turn, the circular resultant of `k * theta` over all
4,318 instances is:

| k | 1 | 2 | 3 | **4** | 5 | 6 |
|---|---|---|---|---|---|---|
| R | 0.7035 | 0.7020 | 0.6913 | **0.9757** | 0.6822 | 0.6839 |

**The fourth harmonic and only the fourth.** Two null models, 2,000 trials each:

```
uniform random fields with R(4t) >= 0.9757   :  0 of 2000  (best 0.0423)
same multiset, angles reshuffled              :  0 of 2000
```

And the three populated orientations sit at **79.083, 169.157 and 259.218
degrees** — gaps of **90.074** and **90.061**. That is a right-angle street grid
rotated 79.33 degrees off the axis, which is what a city is and what a random
field is not.

The remaining values are not noise either: `0xCC`, `0xD4`, `0xEC`, `0xF0` carry
60, 6, 12 and 14 instances at angles off the grid — the buildings that do not
face a street.

## A null model I got wrong, and replaced

The first control I wrote took the four heaviest 256-buckets and measured how far
their gaps sat from 90 degrees. It reported **503 of 2000** random fields scoring
at least as square — which reads as "this is unremarkable" and is simply a broken
test: a uniform field's bucket counts are noise, so "the top four buckets" are
four arbitrary positions, and one of the four I picked was a 20-instance cluster
rather than a grid direction.

The resultant is the right instrument because it uses every instance and needs no
bucket choice. I replaced the test rather than reporting its number.

## And a visual control that failed to decide

Both `+theta` and `-theta` produce a four-fold set, so the distribution cannot
choose between them. I rendered the city three ways — unrotated, `+`, `-` — from
directly above, expecting the street grids to settle one of them.

**They did not.** At that scale the three pictures differ in detail and none
reads as obviously right, and the long thin slabs that dominate a top-down view
are roads whose orientation I cannot judge by eye.

So the sign stays unestablished, `tools/mission01_city_render.cpp` keeps a
rotation switch that defaults to **off**, and no rotation is applied anywhere in
the product. A control that is run and comes back undecided is a result; a
control that is run and then read as confirmation is how cycles 1428, 1440 and
1441 went wrong.

## Under test, not just in a report

`four_fold_resultant` is in the product and the test asserts **the comparison**,
not the number: `R(4t) > 0.95` *and* clear of all five neighbouring harmonics by
0.2. One number cannot show that the structure is specifically four-fold, and a
decoder that mangled `tag_high` would fail the second clause first.

## Not established

- The **sign** of the rotation, and the **scale**: any reading under which the
  data is four-fold works, and "65536 is one turn" is the standard convention
  rather than a derivation from this image.
- The header record's third word; `CMapManager+0x30`.
- Which of the 178 part names each of the 173 ids selects.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
contract_addresses                    pass cited=317 supported=317
contract_derivations                  pass behaviours=52 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Settle the sign with geometry rather than with the eye.** The parts are
modelled axis-aligned and placed on a grid; under the correct sign, neighbouring
instances' rotated footprints should interlock rather than interpenetrate, and
that is a number — total pairwise overlap area under `+theta` against `-theta`
over all 4,318 instances. It is the same shape of test as the land/water control,
and unlike a picture it cannot come back "looks about the same".
