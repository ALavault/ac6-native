# Cycle 1170 — the population closes at 522, and the arithmetic checks itself

## The last 86

Cycle 1166 decoded 436 of the 522 record slots and left the **header textures**
out: a pack's file-level payload spans every texture it holds, so texture 0 needs
its own slice rather than the whole tail.

Sliced the same way the records were — the file's own descriptor, its own surface
size, its own data offset:

```
header wrappers written   86
decoded                   86
refused                    0
```

## The arithmetic closes on itself

A pack declaring **N** has N record slots, of which the last is a terminator
(cycle 1166). So it holds N−1 record textures plus the one its file header
describes — **N textures, from N slots.**

Summed over the 86 packs:

```
record slots            522
terminators              86
record textures         436
header textures          86
total textures          522
```

522 textures from 522 slots, and 522 was independently the GIDX census taken by
regex back in cycle 1157, before any of the structure was understood. Three
routes to the same number: counting `GIDX` occurrences, walking the slots, and
summing the declared counts.

**All 522 decode**, with the product decoder unmodified.

## What they are

Aircraft and vehicle atlases at 2048×2048 and 1024×1024 — panel lines, hatches,
intakes, ship hulls seen from above — plus the Gracemeria building facades cycle
1162 found. Mostly desaturated, which is what a base-colour atlas looks like when
tint arrives from the material.

Shown for inspection. No parity claim, no capture offered as visual parity, and
the pixels stay local: they are retail art in a different encoding.

## What is still not derived

Unchanged from cycle 1167, and worth restating because the completeness of the
extraction is not completeness of the evidence:

- the `0x50` spacing between consecutive sub-records, and the terminator
  convention — measured across 86 packs, not read. The sibling walker has not
  been found;
- the 8-in-16 byte swap — now measured on two populations (cycle 1169) but still
  not read;
- the mip levels above zero, which nothing decodes and nothing consumes.

A complete extraction built on two measured conventions is a complete extraction
built on two measured conventions. It is not a derivation, and the difference is
the whole discipline.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
