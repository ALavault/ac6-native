# Cycle 1348 — 306 vtables with no name

## Qualification

- Ghidra was used once, to read the program's block table. **No oracle pass.**
- No product C++ changed, no contract changed.

## The sweep failed its control, twice, before it passed

Splitting `.rdata` into vtables needs a `.text` range, and I used one observed
from where functions happen to live: `0x82070000` upward. The control — recover
the 811 known bases as run starts — came back **297 of 811**, and the 39
candidates it produced were discarded unread.

A section boundary is a fact the program file carries. `Ac6MemoryBlocks.java`
reads it:

```
.rdata   82000400..82079DD3   r--
.text    82090000..823D772B   r-x
BINK     823D7800..823E7FF7   r-x
```

**Wrong at both ends.** `.text` starts at `0x82090000`, not `0x82070000`, and
`.rdata` runs 40 KB past where I stopped scanning. With the real bounds the
control reads **811 of 811**.

That is the third time in this thread that guessing a boundary produced a number,
and the third time the number was wrong: cycle 1334's 91 slots, cycle 1337's 31
slots, and now 297 of 811.

## The answer to cycle 1347's question

```
runs of .text pointers in .rdata   1117
  named by a Complete Object Locator  811
  UNNAMED                             306
```

**306 vtables — 27% — carry no RTTI.** The class map is complete over what it
covers and covers three quarters of the classes. Every "complete enumeration over
the named vtables" this campaign has published was complete over 73% of the
population, and now that fraction is a measured number instead of an assumption.

## One candidate, from an intersection of two measured properties

Of the 306, twenty-four have at least 51 slots with real `.text` at `+0xC0`,
`+0xC4` and `+0xC8`. Intersecting with the other measured property — constructed
by something that calls `galib::CGaObj`'s constructor, which is what puts a
`CGaLocator` at `+0x60` — leaves **one vtable**:

```
0x820078D0   169 slots   installed by five constructors
   +0xC0 -> 0x82299548     5 instructions,   0 uses of f1
   +0xC4 -> 0x82299560    27 instructions,   2 uses of f1
   +0xC8 -> 0x8229CD78   617 instructions,  16 uses of f1
```

The third slot is a 617-instruction function using the float sixteen times. That
is where the arithmetic went.

## And the repository already knew this vtable

`INSTRUMENT_DISCIPLINE.md` records `0x820078D0` three times, from work that had
nothing to do with this thread:

- it is the **mis-attribution warning** in the file's opening — it shares 11 of 96
  slots with the real `galib::CGaObj`, which is how it once acquired that label;
- objects carrying it are reached **from a factory slot `+0x14` in a loop**;
- **it holds zero at `vtable-4`.**

That last one is this cycle's sweep, written down years of cycles earlier from a
different direction: zero at `vtable-4` *is* "no Complete Object Locator". Two
independent observations of the same absence.

## What is not claimed

**It is not named.** It has no RTTI, so it has no name, and the one it was once
given was wrong — which is why that mistake is the first example in the
discipline file.

**It is not proven to be the child.** It matches both measured properties and
nothing else in 1117 vtables does, which is strong; what would settle it is
showing that one of the five constructors' objects reaches a unit's array at
`+0xD8`. That is the next cycle, not this one's conclusion.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Whether an object with vtable `0x820078D0` reaches `unit+0xD8`. The five
constructors are a bounded population and the factory slot `+0x14` is already
recorded, so this is a link to trace rather than a space to search.
