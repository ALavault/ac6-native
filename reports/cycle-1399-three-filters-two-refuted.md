# Cycle 1399 — three filters, two refuted

## Qualification

- **No Ghidra run and no oracle pass.** The corpus and the class map.
- No product C++ changed; ctest stays 47. **No contract entry.**
- New artefact `analysis/flight/command-caller-search.tsv`; new shape 38.

## The question

Cycle 1398 wired a demo controller to the contracted flight chain through **one
invented link** — the conversion from a binding output to the command setters'
target-and-increment — because who calls slots 12, 13 and 14, and with what, had
never been searched. This is that search. It did not land, and the way it failed
is worth more than the four candidates it produced.

## Filter 1: the class-family filter returns nothing, correctly

The filter that has worked all campaign — restrict to methods of the flight-model
family — returns **zero** from 98, 132 and 151 dispatches.

That is not a failure of the tool. **The steering code is not a flight model**,
so filtering to the flight-model family excludes precisely the callers being
looked for. The instrument was right and the question was wrong for it, which is
a distinction worth making explicitly given how often this session has had it the
other way round.

## Filter 2: four candidates, and two of them are switch statements

"Dispatches all three offsets" gives four. Requiring all three **on the same
object register** gives the same four — which felt like confirmation.

Reading them refutes all four:

- **`0x82220D20`** compares a value and branches to one of
  `lwz r11,36(r11)` / `40` / `44` / `48` / `52` / `56` / `60`, each followed by
  `b 0x822210AC` — **one shared `bctrl`**. It does not call seven virtuals; it
  calls **one**, chosen from a menu.
- **`0x821222B0`** is the same shape, and is slot +0x20 of **`CNuSound`**.
- **`0x822389D8`** reached the list through the same miscount.
- **`0x822B7200`** calls its three in sequence but passes **no float arguments** —
  `mr r3,r30` straight to `bctrl`, returns tested as booleans. The setters take
  `(this, f1, f2)`.

**The flaw was the filter.** A switch over virtual slots is textually
indistinguishable from a caller of several of them, and the "same object
register" refinement makes it *more* convincing rather than less, because a
switch naturally uses one object throughout. The second filter did not test the
first; it re-measured the same artefact and agreed with itself.

That is the **thirty-eighth shape**, indexed.

## Filter 3: the signature, which should have been first

The setters take `(this, float, float)`. Requiring **both `f1` and `f2`** to be
set around the dispatch gives **thirteen sites in eight functions** — and
excludes every switch arm automatically, because a jump table does not marshal
arguments per arm. It needed no class-map filter at all.

| function | insns | sites | offsets | callers |
|---|---:|---:|---|---:|
| **`0x82290E20`** | 241 | **4** | 48, 56 | 1 |
| `0x822911E8` | 538 | 3 | 56 | 4 |
| `0x82290500` | 423 | 1 | 56 | 3 |
| five others | ≤589 | 1 | | ≤1 |

`0x82290E20` is the strongest on shape alone — four sites across two different
setters, one caller. **None of the eight has been read.**

The general lesson is cheaper than the specific one: **a slot number is a
coincidence waiting to happen; an argument list is a claim about the callee.**
Three offsets shared by 78–151 unrelated functions told me nothing. Two float
registers cut it to thirteen sites in one pass.

## What this leaves

The invented link in `demo_flight_input.h` **stays invented**, and the header
still says so. Nothing was weakened to make this cycle produce an answer, and
picking one of the four refuted candidates would have been easy and wrong.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 3 (1395, 1396, 1399) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
instrument_discipline_index           pass shapes=29 unindexed=0
```

## Next

Read `0x82290E20` — 241 instructions, four sites across two setters, one caller.
If it passes an angle and an increment derived from something input-shaped, the
last invented link in the input path becomes a derivation. If it does not, the
next of the eight is 538 instructions with four callers, and the population is
small enough to exhaust.
