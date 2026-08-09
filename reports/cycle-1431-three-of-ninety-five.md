# Cycle 1431 — three of ninety-five

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus and the payload.
- `ScenarioObjScalars` gains a field; ctest stays **53**. **No contract entry** —
  the scenario parser's own entry is not reopened here.
- `ndxr-mission-placed` regenerated with headings.

## The heading, derived rather than inferred

Cycle 1429 named it as the one obviously wrong thing left. It is at **`+0x10`**
of the Obj data block — the word after the three position floats and one zero —
and retail's own consumer settles everything about it:

```
0x8229B0B0  lwz   r11,388(r3)     the Obj data block, entity+0x184
0x8229B0B4  lfs   f1,16(r11)      THIS
0x8229B0B8  fcmpu cr6,f1,f0       against 0.0
0x8229B0BC  beq   -> skip         zero means NO ROTATION, explicitly
0x8229B0C0  addi  r3,r3,96        the entity's transform
0x8229B0C4  bl    0x820A9B30      rotate_820A9B30
```

`0x820A9B30` is one of the three rotations A3.1 ported and contracted. **Which
axis the heading turns about is settled by which rotation retail passes it to**,
not by a guess — the same question the demo's own basis convention has been
unable to answer for itself since cycle 1407.

It is the **middle of a triple**: `0x8229ADF8` reads `+0x0C`, `+0x10` and `+0x14`
together at `0x8229AF50`. In Mission 01 the outer two are zero in all 434 Obj
records.

Fourteen distinct values, all clean radians: ±π, ±π/2, ±π/4, ±π/6, −5π/9, and
degree-round angles — −145°, −100°, −80°, −70°, −60°, −50°. Authored in degrees,
stored in radians.

## And the fix is smaller than the complaint

Cycle 1429 wrote that every unit facing the same way was "wrong, and visible".

**Only 3 of the 95 placed units carry a non-zero heading.** The other 49 of the
mission's 52 belong to units with no load-time position, which are not drawn.

So the sameness was mostly **retail's arrangement**, not this renderer's
omission. I had assumed a missing read where the payload actually says zero
ninety-two times, and zero is retail's own "do not rotate" — tested explicitly
at `0x8229B0B8` before the call.

That is worth more than the three units it moved: a defect I named confidently
in a report turned out to be four-fifths a property of the data.

## How it was found, and the method held

Cycle 1426's rule was *look at the bytes before enumerating readings of them*.
This cycle dumped sixteen words of the Obj block across all 434 records and
counted non-zero per word before proposing anything:

```
word  0 : 190     word  3 : 0      word  4 : 52     word  5 : 0
word  1 :  60     word  6 : 420    word  7 : 0
word  2 : 222
```

Word 4 and nothing else. Then `−0.785398` named itself, and only then was the
consumer searched for — a dataflow query for a `lfs +16` off a register loaded
from `[x+388]`, seven sites, one of them in the same function that reads the
position.

**Bytes, then a value, then the code that reads it.** Three cycles ago the order
was reversed and cost three failed arbitrations.

## Not established

- Why `0x8229ADF8` reads the full triple when only the middle is ever non-zero
  here. Another mission may use all three.
- Whether the default orientation — heading zero — points the models the way the
  game shows them. That is a claim about the model's own axes, which nothing
  here reads.
- The remaining 135 unplaced units, unchanged.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
capture_images_match_metrics          pass compared=4
```

## Next

**The decoder's contract entry**, which has now waited four cycles on
`0x821FBB10` and `0x821FBA78` — the two buffer creators that would turn the
section assignment from a cross-match into a derivation. Everything built on top
of it since has held, which is a reason to close it rather than a reason to keep
deferring.
