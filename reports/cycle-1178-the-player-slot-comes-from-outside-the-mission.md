# Cycle 1178 — the player slot is written outside the mission load

## Where the slot comes from

`global+0x4B40` is the player slot: the index all three `PLAD` call sites use to
pick a record. Every reference to it in the image:

```
read   0x82097FC4   0x8219C83C   0x821A0324      the three PLAD sites
write  0x82199FC4   0x821A7344
```

Three readers, two writers, and **no writer is in the mission loader.**
`0x82199F68`, which holds the first, is a state-transition handler: it branches
on state codes (`r4 == -3`, `-2`), reads a byte out of the mission-key object at
`*(0x8293BA10)`, and tests the game mode at `*(0x826E4EB4) + 0x78` — the same
field the earlier trace found `0x821B6E58` branching on for the resource id.

So the slot is decided by game state before the mission is loaded, and the loader
consumes it. That is what cycle 1177 predicted from the shape of three empty
searches, now with a call graph behind it instead of an inference.

## What this settles and what it does not

**Settles**: the player's configuration is not in the scenario container, and the
three searches that came back empty were not looking in the wrong place — there
is nothing there to find. The mission is handed a slot index from outside.

**Does not settle**: what the slot indexes into, or where the player's aircraft
and spawn actually come from. `0x82199F68` writes the slot; it does not obviously
write a model or a position. The next question is what reads the slot besides
`PLAD` — and that is a search this cycle has not run.

It also does not vindicate `PLAD`. Cycle 1177 established exhaustively that its
floats have no reader through either accessor, and a slot arriving from game
state does not change that: the slot picks *which* record, and the record's
floats are still read by nothing.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
