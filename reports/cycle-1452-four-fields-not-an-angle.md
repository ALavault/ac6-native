# Cycle 1452 — four fields, not an angle

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`, the
  archive, and the ported decoders.
- Product C++ **changed**: `MapInstance` gains `selector`, `quadrant`, `kind`,
  `accepted`; the placement test gains six assertions. ctest stays **56**.
- Contract: `retail_map_placement`'s statement, addresses and native-test claim
  updated. Still **34 behaviours**.
- New: `tools/audit_placement_rotation_sign.cpp`.

## The test I set out to run, and its refutation

Cycle 1451 named the next step: settle the rotation's sign by geometry rather
than by eye — total footprint overlap, or overhang into the bay — because "unlike
a picture it cannot come back 'looks about the same'".

It came back worse than that. Sampling each part's real vertices, rotating by
`+theta`, `-theta`, none, and a random angle, and asking the ported water grid
how much lands on water:

```
                        all instances     coastal only (286)
no rotation                  1.724%            27.623%
+theta                       1.702%            27.265%
-theta                       1.646%            26.365%
random angle (null)          1.634%            26.187%
```

**The null wins.** A random angle puts less of the city in the water than either
real sign. So the proxy is not blunt, it is *refuted as an instrument*: it does
not measure what I wanted it to measure, and no result from it could have been
trusted. The tool says `UNDECIDED` and returns rather than picking the smaller
number, which is the only reason this cycle did not publish a sign.

Then I stopped fitting the data and read the code.

## The tag is four fields

`0x82102340` masks **three** bits and `0x82102364` masks **nine** —
`rlwinm r29,r30,16,23,31`. The high half is not one number:

| bits | | Mission 01 |
|---|---|---|
| 0..15 | a value | 0..172 |
| 16..24 | **nine bits**, the selector | 8..169, 160 distinct |
| 25..26 | zero on every accepted record | |
| 27..29 | the kind; `0x82102350` accepts **only 0 or 7** | 7 on 4,226, else 1/2/5/6 on 92 |
| 30..31 | two bits | 345 / 584 / 3277 / 20 |

**Retail skips 92 of the 4,318 records.** Nothing before this cycle knew that.

## Correcting cycle 1451, which measured a real thing and explained it wrongly

Cycle 1451 found R(4t) = 0.9757 on `tag >> 16` read as a fraction of a turn,
against 0.68–0.70 at every other harmonic and zero of 2,000 trials under two null
models. **The statistic is real and reproducible.** Its explanation is not.

The four-fold structure is the **two-bit field in bits 30..31**. Each step moves
`tag >> 16` by exactly `0x4000` — a quarter of the u16 range — so four values of
two bits are 90 degrees apart *by construction*. There is no angle. The "street
grid rotated 79.33 degrees" was the mean of the remaining fourteen bits and means
nothing.

That is a shape worth naming: **a statistic with a null model can still be
explained by the wrong mechanism.** Two independent nulls, 4,000 trials, and none
of it touched the question of *why* the data is four-fold. The fix was not a
better null; it was reading the instructions that consume the field.

`four_fold_resultant` stays in the product and stays under test, because the
number is true. The header now says what it measures.

## And a doubt about three cycles of pictures

`0x82102378` passes the **nine-bit** field to vtable slot `+0x5C`
(`0x82100600`), which is

```
lwz    r11,0x74(r3)      the count
cmplw  r4,r11            refuse if the index is not below it
addi   r11,r4,0x1B63
lwzx   r3,r11,r3         this->table[0x1B63 + index]
```

— a bounds-checked resource table, and `parts/%d` sits in the same string table
as the `.pdl` name. So the nine-bit field is retail's part selector.
`tag & 0xFFFF` is extracted separately at `0x821023B4` and used for something
else.

`tools/mission01_city_render.cpp` selects models with `tag & 0xFFFF`. Every
instance resolved to a file — but there are 256 files and the ids run 0..172, so
that was never evidence, and cycle 1449's report said so at the time. **The
renders from cycles 1449–1451 may be drawing the wrong buildings.** Recorded, not
quietly fixed: which of the two fields names a model is not established, only
that retail hands the nine-bit one to a table lookup.

## Not established

- Which field selects the model. The nine-bit one is what retail indexes with;
  what `tag & 0xFFFF` is for is unread.
- The sign of anything. There is no angle to have a sign.
- What `this+0x6D8C` holds and who fills it; `this+0x74`.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
contract_addresses                    pass cited=320 supported=320
contract_derivations                  pass behaviours=52 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

Three of this cycle's six new assertions failed on first run, all for the same
reason: I had computed the field statistics over the accepted records and
asserted them over all 4,318. The 92 skipped records carry different values, and
the test now keeps the two populations apart deliberately.

## Next

**Settle which field names a model, by drawing both.** Render the city selecting
with `tag & 0xFFFF` and again with the nine-bit selector; the wrong one puts
towers where sheds belong and roads on end. That is the same visual test that
failed to decide a rotation sign — and it will work here, because these two
readings differ by whole buildings rather than by a reflection.
