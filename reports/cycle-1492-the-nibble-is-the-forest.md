# Cycle 1492 — the nibble is the forest

## Qualification

- **No Ghidra run and no oracle pass.** The archive.
- No product C++ changed; ctest stays **60**. **No contract entry.**
- Lands before cycle 1489's pending video commit, like 1490 and 1491.

## The contradiction 1490 left standing

`0x82108440` reads a mask nibble and indexes a table at `[r9+0x4C]` with it —
a *selector*. But cycle 1490 measured the points sitting on nibble **0** at
99.1%, and zero cannot select anything. If the nibble chose each point's tree,
the points would sit on non-zero nibbles. They do the opposite.

## The measurement that resolves it

The nibble alphabet over all 24 blocks of `013`:

```
0 : 80.95%      1 : 15.68%      2 : 0.03%      3 : 3.07%      5 : 0.27%
```

And the mapset's `.tree` groups (cycle 1482, verbatim): **g1, g2, g3, g5 carry
non-zero `(Kind, SetNo)` presets; g4, g6, g7, g8 are all-zero.**

> Nibbles used: `{1, 2, 3, 5}`. Groups populated: `{1, 2, 3, 5}`. **Exact, both
> directions** — every nibble that appears has a populated preset group, and
> every populated group appears in the mask.

By chance, four used nibble values landing on exactly the four populated groups
of eight is 1 in 70 before even counting the eleven unused nibble values.

## The model, stated

- **`013` (the wsd) paints procedural vegetation regions by group**: nibble
  `g` means "this 64-unit square grows group `g`'s trees" — g1's 15.68% is the
  map's forests, g3 the sparser cover, 0 means none. The `[r9+0x4C]` table in
  the reader is the group -> preset lookup, which is why the code indexes with
  the nibble.
- **`012` (the wpd) is individually placed trees**, and they are *excluded*
  from the painted regions — 99.1% on nibble 0 — because a hand-placed tree
  inside a procedural forest would double up.

The exclusion reading of 1490 and the selector reading of the code were both
right; they are about different trees.

## Not established

- Density and scatter within a painted region — `scale1..8 = 25,15,15,25,...`
  and `FadeDistance 8192` are read but their use is not.
- The `(Kind, SetNo) -> model` resolution, unchanged.
- Which preset of a group's three applies where.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

Still cycle 1489's video the moment the render completes; after it, the
vegetation can go into the sequence as two layers matching the two files —
painted-region fill and placed singles — instead of one undifferentiated
scatter.
