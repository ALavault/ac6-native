# Cycle 1347 — 811 of 811

## Qualification

- **No Ghidra run and no oracle pass.** The image was read directly.
- No product C++ changed, no contract changed.

## The class map is complete, and now that is measured

Cycle 1346 ended by naming a risk larger than its own thread: *"every 'complete
enumeration over the named vtables' this campaign has published rests on 811
being most of them, and nothing has checked it."*

It is checked. An independent sweep, written from cycle 1335's structural
description rather than from the map — scan every aligned word, keep those
pointing at a structure whose signature is zero and whose type descriptor spells
a name beginning `.?A`, and take the word's own address plus four as a vtable
base — finds:

```
vtables located by their COL   811
class-map entries              811
located and mapped             811
located but NOT in the map       0
in the map but not located       0
```

**Exact agreement, both directions.** Two methods sharing no code and no
assumption beyond the RTTI layout itself. Every enumeration this campaign has
run over the class map was complete over the classes that carry RTTI.

## Which also gives exact widths

Vtables are packed `COL | slots | COL | slots`, so where two are adjacent the
slot count is `(next_base − 4 − base) / 4` — exact, not the upper bound cycle
1338 had to settle for.

Re-running cycle 1346's interface enumeration with exact widths **and stubs
allowed** — a deliberate loosening, since excluding stubs could have discarded a
real class — gives **fifteen** candidates. Still every one a manager, an
interface, a sound or a text service. **None is a per-object type.**

The limit of "exact": it is exact only where the two vtables are adjacent.
`Morphing_PNBT` comes out at 3328 slots because something else sits in the gap —
the same script-binding data cycle 1338 found. The number is right for the
packed case and meaningless otherwise, and the fifteen were read, not trusted.

## So the child has no RTTI

Two enumerations over the 811 — by interface and by layout — find no fit, and the
811 is now provably every class that has a Complete Object Locator at all.

**The child's vtable therefore carries no RTTI.** That is a property of the
binary, not a gap in the tooling, and it explains why five cycles of class-map
lookups came back empty. MSVC emits a COL for every polymorphic class compiled
with RTTI on; a translation unit compiled without it produces vtables that no
amount of map-reading will name.

## The next instrument is the one I got wrong

A COL-less vtable is a run of `.text` pointers in `.rdata` **not** preceded by a
valid locator. That is exactly the technique cycle 1334 misapplied when it read
`CGaLocator` as 91 slots — the method was sound and the subject was wrong. For a
class the map already names, the map is the answer; for a class it cannot name,
the run of pointers is all there is.

The twenty-ninth shape says the extent comes from the map. It now needs its own
companion: **when the map has nothing to say, the bytes are the only source, and
the COL sweep says exactly where the map's knowledge stops.**

## Not established

- How many COL-less vtables the binary has.
- What class the children are.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Sweep `.rdata` for runs of `.text` pointers with no COL in front, and intersect
with the child's two measured properties: a `CGaLocator` vtable stored at `+0x60`
by whatever constructs it, and real functions at slots `+0xC0`, `+0xC4`, `+0xC8`.
Both are already enumerable; only the population was wrong.
