# Cycle 1346 — the child is not in the class map

## Qualification

- **No Ghidra run and no oracle pass.** The image was read directly, and the flat
  mapping was verified against three values Ghidra had already produced.
- No product C++ changed, no contract changed.

## A faster instrument, checked before it was used

`analysis-input/ACE6_X360.exe` maps flat at base `0x82000000`, so vtables can be
read in Python rather than through a Ghidra invocation. That is worth a control
before it is worth anything else, so three words Ghidra had already reported were
re-read from the file: `[0x82054D94] = 0x82093818`, `[0x82056874] = 0x820A6FB8`,
`[0x8200082C] = 0`. All three match.

That turned this cycle's enumerations from minutes into milliseconds.

## Two complete enumerations, and neither finds the child

The child is called through slots `+0xC0`, `+0xC4`, `+0xC8` (cycle 1341) and
carries a `CGaLocator` at `+0x60` (cycle 1340). Both are enumerable properties.

**By interface.** Of the 811 named vtables, 43 have room for a slot at `+0xC8`
before the next named vtable. Of those, **nine** hold three distinct non-stub
functions there — and all nine are managers: `CMapManager`, `CNuSound`,
`CSelectAircraftManager`, two effect managers, `CX360MissionManagerOnline`, and
so on. None is a per-object type.

**By layout.** `0x82054D94` is materialised at 127 sites; of those, **ten** store
it at displacement `0x60`, and four of those ten write `96(r1)` — stack locals,
not members. Four constructors genuinely embed a locator at `+0x60`, and the
classes they build are `CX360ObjManagerThread`, `CDebriefingCamera`,
`CDebriefingManager`, and **`galib::CGaObj`**.

`CGaObj` is the interesting one: it is a base class, so **every class deriving
from it inherits a locator at `+0x60`** — which is exactly the offset cycle 1340
measured on the child and exactly what distinguishes it from `CAce6Unit`, whose
own locator is at `+0x80`.

**By both.** Constructors that call `CGaObj`'s and whose class has three real
functions at `+0xC0`…`+0xC8`: **one**, `ACE6::CAce6EffectManager`. An effect
manager is not a thing a player unit copies its pose from.

## So a premise is wrong, and it is a stated one

The two enumerations are complete over the population they name: **the 811
vtables whose RTTI chain resolved**. The class map's own header says an entry
appears only then.

The conclusion is therefore not "the child does not exist" but **"the child's
vtable is not in the class map"** — its RTTI did not resolve, or its constructor
reaches `CGaObj` through an intermediate base this filter did not follow.

That is a real result from a bounded population, and it is the opposite failure
from the last four cycles: those had scans returning lists that could not be
narrowed. This is an enumeration returning almost nothing, which narrows the
question to *where the population itself is wrong*.

## Not established

- What class the children are.
- How many vtables the binary has that RTTI never named. The class map has 811;
  nothing has counted the rest.
- Whether the child reaches `CGaObj` through an intermediate base.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Count the unnamed vtables. A vtable is a run of words pointing into `.text`
preceded by a Complete Object Locator, and cycle 1335 established that shape
precisely — `COL | slots | COL | slots`. Sweeping `.rdata` for COLs whose type
descriptor resolves gives the true population, and the difference between that
and 811 is how much of this binary the class map has been silently not covering.

That is worth knowing well beyond this thread: every "complete enumeration over
the named vtables" this campaign has published rests on 811 being most of them,
and nothing has checked it.
