# Cycle 1480 — the join under test

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ **changed**: `MapInstance::quadrant` renamed `draw_class`, and the
  class join added to the placement test. ctest stays **59**.
- Contract: `retail_map_placement`'s statement and native-test claim corrected.

## The correction, carried into the contract

Cycle 1479 established that the tag's bits 30..31 are the draw-distance class,
not the rotation quadrant cycle 1452 read them as. That correction now lives
where the auditor reads it — the derivation header and the contract statement —
rather than only in a report.

The join is **asserted in `ctest`**, not cited:

```
class join: l+airport 345/345  m 584/584  s 3277/3277  x 112/20
```

Four buckets from two files parsed by different code: the tag counts come from
the placement list, the name counts from opening 170 NDXR containers and reading
their record names. Nothing is fitted.

## Three readings of four values, and what the lesson turned out to be

- 1451: a four-fold statistic, explained as a street grid at 79.33 degrees.
- 1452: corrected the mechanism to "a two-bit field", left the meaning open,
  and wrote the shape *a statistic with a null model can still be explained by
  the wrong mechanism*.
- 1479: the meaning, by an external join.

1452's own lesson applies to 1452: **a mechanism is not a meaning.** Knowing the
data is two bits says nothing about what the two bits select, and it took a count
from a different file to say.

## The complexity gate earning its place

Adding the join inline pushed the test's `main` to **238 lines against a 220
budget** and `ac6-cpp-complexity` failed the suite. Extracted to
`check_class_join`, which is where it belonged anyway — a join between two
histograms is a thing with a name.

Worth recording because the gate is cheap and easy to resent: it caught real
bloat introduced in the same edit that added the evidence.

## Not established

- Which record an `x`-class instance draws when its model holds both `l` and
  `x`. Model **163** is the only one of 170 with two class tokens, so the
  renderer's rule rests on one example. Unchanged from 1479.
- Whether the class selects a mesh level of detail as well as a distance.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
contract_addresses                      pass cited=321 supported=321
contract_derivations                    pass gaps=0 multiple=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Fold in the parallel sweep.** Six investigations are running over the material
parameter chain, the terrain texture, the `.sph` sky, the tree system, the
unexplained containers and the mapset's unused groups; its synthesis names the
cheapest next cycle and the one that most changes what a render looks like.
Choosing before it lands would be choosing by momentum.
