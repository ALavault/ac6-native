# Cycle 1455 — thirty-five, not thirty-one

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`.
- No product C++ changed; ctest stays **56**. **No contract entry.**
- `tools/audit_instrument_discipline_index.py` fixed;
  `INSTRUMENT_DISCIPLINE.md` gains one shape and two index rows.

## Following the last field

`tag & 0xFFFF` is extracted at `0x821023B4` and passed as **`r9`** — the seventh
integer argument — to `0x822C2868`, together with the `.nud` resource in `r7`,
`r14` from `0x82102148`'s own parameters, and five stack buffers holding the
transform built at `0x80`..`0xD0(r1)`. So it is a **draw argument**, not a
selector. Inside, `0x822C28A8` moves it to `r27` and `0x822C28C8` parks it at
`0x234(r1)`; following it further is another read.

`0x822C2868` is **566 instructions** by `.pdata`, and its decompilation is
`halt_baddata()` at the second one. That is the sharpest case this campaign has
for why `tools/ppc_read.py` exists.

One more gate turned up on the way: `0x8210244C`..`0x8210245C` compares the
nine-bit selector against `[this+0x74] − [this+0x78]` — the two counters the
loader keeps for the `.nud` and the plain `parts/%d` loads — and instances below
that boundary are abandoned with `2` written to an out-parameter. So the two
counters partition the parts into classes at draw time.

## And the checker that was counting a word

Adding the cycle-1452 lesson as a shape meant running
`audit_instrument_discipline_index.py`. It printed `shapes=31 unindexed=0`,
unchanged — because it selects sections with

```python
headings = [h for h in re.findall(r"^## (.+)$", text, re.M) if "shape" in h.lower()]
```

**That is not a test of whether a section is a shape. It is a test of whether its
author wrote the word.** The file holds **39** second-level headings; the checker
looked at 31. Of the eight it never saw, four are structural — and **three are
real shapes**:

- *What made the eighth different* — validate a zero-hit search against a case
  whose answer you know;
- *The Xenon project has no reference database, and it fails silently*;
- *And a corollary about call sites* — seven literals are not a census.

The fourth was the section I was adding, which is how I found it: the checker
that exists to catch an unindexed shape was structurally blind to every shape
whose heading reads naturally, and its own `shapes=31` was reporting a word
count. `CLAUDE.md` cites that number.

**Fixed by inverting the default**: every heading after the index boundary is a
shape unless it is on a named `STRUCTURAL` list. And `short_name` now drops a
leading `and `, because a heading may open with a conjunction where an index
entry does not — that alone hid one shape.

The corrected checker immediately failed with two unindexed shapes, one of which
had never been indexed at all. Both are indexed now:

```
instrument_discipline_index=pass shapes=35 unindexed=0
```

**Thirty-five, not thirty-one.** Four shapes were written and unfindable by the
reader this file is for, for as long as the checker has existed.

## The shape added

*The right number, the wrong mechanism.* Cycle 1451's R(4t) = 0.9757 over 4,000
null trials was correct and reproducible and described a two-bit field, not an
angle. A null model establishes that a pattern is real; it cannot say why the
pattern is there, and a huge margin makes the attached story feel proportionally
certain when it licenses nothing about it.

## Not established

- What `0x822C2868` does with `tag & 0xFFFF`.
- `this+0x40B0`, the plain `parts/%d` table, and what a `.nud` is against the
  `.ndxr` the container holds.
- Whether `CLAUDE.md`'s other cited counts are measured the way they read.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
contract_artifacts (three live)       pass  cited=131 match_head=131
contract_addresses                    pass  cited=321 supported=321
instrument_discipline_index           pass  shapes=35 unindexed=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Audit the other numbers `CLAUDE.md` cites the way this one was audited.** It
states 138/138 for the harness calibration — a figure that was 0/138 for
eighty-seven commits while the file said otherwise, and cycle 1414 fixed the
comparator rather than the claim. `shapes=31` has now been wrong for as long as
the checker existed. A number in that file is load-bearing precisely because it
is read instead of re-measured, and two of them have now been wrong.
