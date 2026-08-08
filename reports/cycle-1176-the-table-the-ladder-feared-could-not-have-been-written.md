# Cycle 1176 — the table the ladder feared could not have been written

## The fear, stated precisely

`MISSION01_LADDER.md` has warned since the plan that step 2f is *"where JV would
quietly become J1 again"*: a hand-written class→model table satisfies the auditor
— it is native source, it cites no generated code — while destroying the property
the auditor exists to protect. Cycle 1172 ported the binding by derivation
instead, and said so.

There is a sharper thing to check. Not *whether* someone would write that table,
but whether it exists to be written.

## It does not

Over Mission 01's 230 unit records:

```
class   units with a model   distinct model indices
  1              33                    23
  2             116                    17

models used by more than one class:  2 of 38
```

Class 2 — 116 units, half the mission — uses **seventeen different meshes**.
Class 1 uses twenty-three. A class byte does not determine a model, so
`class → model` is not a function and no table of that shape can reproduce the
data. Anyone writing one would have had to pick a representative per class and
watch 116 units become the same aircraft.

The converse is nearly clean too: only 2 of the 38 meshes are shared between
classes. Models are near-private to a class, but classes are far from private to
a model.

So the byte at `+0x61` is not a redundant encoding of something the class already
says. It is the only thing that says it.

## The two classes with no model at all

Classes 0 and 4 carry **no model index on any record**. Class 0 is the player —
the record `0x820A7420` classifies as category 2.

That is the second time the player has turned out to be absent from a table
everything else is in: cycle 1145 found it has no tag-2 order and therefore no
load-time position, and cycle 1146 found `PLAD`'s floats have no reader. Its
spawn and its model are both somewhere this port has not looked, and they are
now two instances of one pattern rather than two unrelated gaps.

## Why this is worth a cycle

Because "I derived it rather than fitting it" is a claim about my conduct, and
this is a claim about the data. The derivation would have been the right thing to
do even if a class→model table had been possible; the fact that it was not means
the shortcut was never available, and a future cycle tempted by it can be shown
the numbers instead of being asked to take the discipline on trust.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
