# Cycle 1336 — the RTTI says there is no base

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed; five structures were decoded.
- No product C++ changed, no contract changed.

## `galib::CGaLocator` derives from nothing

Cycle 1335 found the class's Complete Object Locator sits at `vtable[-1]`. Read
end to end, every field checked against the next:

```
0x8206D844  COL        signature 0, offset 0, cdOffset 0
                       typeDescriptor      0x8268F748
                       classHierarchyDesc  0x8206D858
0x8206D858  hierarchy  attributes 0 (single, non-virtual), numBaseClasses 1
                       pBaseClassArray     0x8206D868
0x8206D868  array      ONE entry -> 0x8206D870
0x8206D870  base       typeDescriptor 0x8268F748  — CGaLocator ITSELF
                       numContainedBases 0, PMD (0, -1, 0)
                       back-pointer 0x8206D858
```

MSVC's base-class array **includes the class itself**, so one entry naming itself
with `numContainedBases = 0` means there are no bases. The descriptor also points
back at the hierarchy it came from, which closes the ring: every pointer in the
chain was followed and every one of them landed where the previous structure said
it would.

## Which turns yesterday's withdrawal into a positive result

Cycle 1334 hypothesised multiple inheritance between `CGaLocator` and
`CGaObjDesc` from their adjacency in `.rdata`. Cycle 1335 withdrew it on the
grounds that adjacency is how *every* vtable sits there — a removal of bad
evidence, which is not the same as evidence of the opposite.

This is the opposite. **The RTTI states there is no base**, and it took four
dereferences.

## The limit of that statement

RTTI records what the compiler emitted **for this class**. A class deriving
*from* `CGaLocator` would record the relationship in **its own** hierarchy
descriptor, not here. So this says nothing about derived classes — only that
`CGaLocator` derives from nothing.

That distinction is the whole reason to write the sentence carefully: "no base
classes" and "nothing derives from it" are different claims and only the first
was measured.

## What it settles for the port

Both locators a unit carries are the **same type**, with one virtual method and
no polymorphic variants. So whatever separates `+0x10` from `+0x80` is **usage
and nothing else** — the type question is closed and cannot be the answer.

And `RetailBasis` in `include/ac6/retail_transform.h`, a plain struct with free
functions, is now confirmed as **retail's own shape** rather than a
simplification I chose when I wrote it. That was an open risk in cycle 1328 and
it is closed.

## Not established

- What separates the two locators. Three probes are now spent — the address-of
  scan, the interface, and the type — and each closed a possibility rather than
  finding the answer.
- What writes `unit+0xE0`.
- Anything about classes that derive from `CGaLocator`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
instrument_discipline_index          pass, 20 shapes, 0 unindexed
tools/tests                          Ran 72 tests, OK
```

## Next

Usage is the only remaining axis, and there is one place it is already known: the
four callers of `0x822A1E80` all reach `+0x80`. The symmetric question — what
reads `+0x10` — is answerable the same way the `+0x80` users were found, from the
unit's own methods rather than from an image-wide scan. `CAce6Unit`'s vtable is a
bounded population and its extent now comes from the class map, which is a lesson
this thread paid for two cycles ago.
