# Cycle 1179 — PLAD selects a route, and that is all it does

## The search cycle 1178 named was already answered

It ended by asking what reads the player slot besides `PLAD`. The reference scan
already had the answer and I had not read it that way.

`global+0x4B40` has exactly three readers, and each is the instruction
**immediately before** a `PLAD` getter call:

```
0x82097FC4  lwz r4,0x4b40(r11)   ->  0x82097FC8  bl 0x82249BC8
0x8219C83C  lwz r4,0x4b40(r11)   ->  0x8219C840  bl 0x82249BC8
0x821A0324  lwz r4,0x4b40(r11)   ->  0x821A0328  bl 0x82249BC8
```

Four bytes apart in every case. **Nothing else in the image reads the player
slot.** It is not a general "which player is this" value consulted around the
codebase; its entire consumer set is those three lookups.

## So the whole of PLAD is one field

Chaining what is now established end to end:

```
game state  ->  0x82199F68 writes global+0x4B40      (cycle 1178)
            ->  three sites read it, each feeding    (this cycle)
            ->  0x82249BC8, the PLAD record getter
            ->  each caller reads word 3 only        (cycles 1131, 1146, 1177)
            ->  stored to +0xF0 of the unit at manager+0x404, the route cursor
```

Every link is exhaustive rather than sampled: every reference to the slot, every
call site of both accessors, every getter body. **`PLAD` is a route-selection
table.** The player slot exists to pick a row, and the row contributes one word.

The three floats in each row — Mission 1's `(-2025, 1500, 1345)`, at exactly the
1500 altitude the aircraft spawns use — are read by nothing. Cycle 1143 measured
all fifteen missions and found 33 rows of distinct world-scale positions. They
are authored, they are plausible, and this image does not consume them.

## The trap this closes

Cycle 1146 refused `PLAD` as the player spawn and wrote that the coincidence "is
convincing enough that a later cycle will have the same idea". Cycles 1177 and
1178 each returned to it. The record now says not just *that* the floats are
unread but *why the whole table exists*, which is a harder thing to argue with
than a negative.

A future cycle that finds 33 authored world positions in the archive should read
this before spending a day on them.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
