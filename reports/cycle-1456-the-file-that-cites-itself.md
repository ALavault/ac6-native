# Cycle 1456 — the file that cites itself

## Qualification

- **No Ghidra run and no oracle pass.** The image, the repository, the checkers.
- No product C++ changed; ctest stays **56**. **No contract entry.**
- New: `tools/audit_claude_md_numbers.py`. `CLAUDE.md` corrected in one figure
  and gains a checker of its own.

## Correcting cycle 1455, which invented an attribution

Cycle 1455 wrote, of `INSTRUMENT_DISCIPLINE.md`'s shape count:

> "its own `shapes=31` was reporting a word count. `CLAUDE.md` cites that
> number."

**`CLAUDE.md` does not cite it.** `grep` finds no shape count in the file at all.
The figure came from a session summary I was carrying, and I attributed it to the
governing document without opening it — inside a cycle whose entire subject was a
checker that measured the wrong thing.

Everything else in cycle 1455 stands: the checker really did count the word
"shape", the file really did hold 35 sections where it reported 31, and four
shapes really were unindexed. Only the sentence about `CLAUDE.md` was invented,
and it is the sentence that made the finding sound load-bearing.

## What `CLAUDE.md` actually cites, measured

| claim | measured | |
|---|---|---|
| `0x822A23D8` is 460 instructions | 460 | ok |
| `exports/` recovers **6** of it | **2** | **wrong** |
| `0x82263A50` has three `bctr` | 3 | ok |
| the harness calibration is 138/138 | — | needs a Ghidra pass |

`exports/822a23d8.json` carries p-code for **two** distinct instruction
addresses. Whether the six was wrong when written or the export was regenerated
since cannot be told from here — which is the point of the cycle rather than a
caveat on it. The file now says 2.

## And a mistake made inside the audit

Checking the `bctr` claim, I ran `count_indirect_branches.py` over
`0x82263A50..0x82264000` — a window I picked — and got `bctr=2`, then wrote that
`CLAUDE.md` "conflates `bctr` and `bctrl`".

`.pdata` declares the function as **1,880 instructions**, `0x82263A50..0x822657AC`.
Over its real extent the count is **`bctr=3`**, and the claim is exactly right.
That is *stopping at a natural boundary*, indexed in `INSTRUMENT_DISCIPLINE.md`
since long before this cycle, committed while auditing other people's numbers.
The tool now derives the range from `.pdata` and says so in a comment.

## The checker

`tools/audit_claude_md_numbers.py` re-measures each claim from the image or from
another checker and **fails** on a mismatch. The calibration claim is printed as
`UNCHECKED` rather than omitted: an audit that hides what it cannot reach is the
shape it exists to prevent.

```
claude_md_numbers=pass checked=3 mismatched=0
```

`CLAUDE.md` documents it beside the other checkers, with both rotted figures
named — `138/138` that was 0 of 138 for 87 commits, and `recovers 6` that was 2.

## Not established

- Whether the harness calibration is 138/138 today. It needs
  `analyzeHeadless`, and no cycle since 1414 has run it.
- The historical figures in `CLAUDE.md` — cycle 1273's 4 markers against 29,
  cycle 1261's ten addresses — which describe past states and cannot be
  re-measured from the tree.

## Gates

```
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
claude_md_numbers                     pass checked=3 mismatched=0
instrument_discipline_index           pass shapes=35 unindexed=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Run the harness calibration.** It is the one number in `CLAUDE.md` no checker
can reach, the file's own instructions say to re-run it whenever
`MicroExecuteFunction.java` changes, and the last cycle to touch that file was
1413. Two of the three checkable figures in that file have now been wrong; the
unchecked one has been wrong before, for eighty-seven commits, and nothing in the
repository would say if it were wrong again.
