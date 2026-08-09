# Cycle 1457 — one hundred and thirty-eight

## Qualification

- **A Ghidra run: yes** — `analyzeHeadless`, read-only, `-noanalysis`, the
  calibration batch only. **No oracle pass.**
- No product C++ changed; ctest stays **56**. **No contract entry.**
- `CLAUDE.md` gains the exact invocation and a last-run stamp;
  `tools/audit_claude_md_numbers.py` reads that stamp.

## The number nothing could check

Cycle 1456 audited every figure in `CLAUDE.md` and found one wrong, one right,
one right — and one it could not reach: the micro-execution harness calibration,
because it needs a Ghidra pass and the checkers do not run one.

That figure has been wrong before. `CLAUDE.md` records it: **0 of 138 for
eighty-seven commits** while the file said 138/138, found only because cycle 1413
was checking whether its own change had broken something. Nothing in the tree
would have said.

It was run this cycle:

```
emitted specs=138
AC6_MICROEXEC_BATCH cases=138
cases=138 equal=138 failures=0
microexec_harness_calibration=pass
```

> **138 of 138.** The instrument every other measurement in this campaign rests
> on reproduces the committed corpus exactly, today, on this tree.

Last run before this: cycle 1414.

## What made it expensive, and no longer is

`CLAUDE.md` gave the recipe as three lines, the middle one being
`<analyzeHeadless ... --batch W/manifest>`. Reconstructing that placeholder — the
project path, `-readOnly -noanalysis`, `-process default.xex`, the script path —
took the first part of this cycle and came out of a report from **cycle 70**.

The file now carries the command line. It takes about four minutes.

## And a stamp, because the failure mode is silence

The calibration cannot be run from a checker. What *can* be checked is **when it
last ran**, and a stale stamp is exactly the eighty-seven-commit failure in
readable form. `CLAUDE.md` now records the cycle and the result;
`audit_claude_md_numbers.py` prints them, and **fails** if no run is recorded at
all.

```
harness calibration    last run cycle 1457, 138 of 138 -- rerun after touching MicroExecuteFunction.java
claude_md_numbers=pass checked=3 mismatched=0
```

That is not as good as running it. It is the difference between a number that
rots silently and one that says how old it is.

## Not established

- Whether the 138 cases still cover what the harness does. They were chosen
  before `alias`, `override`, `dump` and the register-file bridge existed; cycle
  1414's fix was to the comparator for exactly that reason. A calibration that
  passes is evidence the harness is unchanged, not that it is well covered.
- The other `CLAUDE.md` figures that describe past states and cannot be
  re-measured.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
microexec_harness_calibration         pass cases=138 equal=138 failures=0
claude_md_numbers                     pass checked=3 mismatched=0
instrument_discipline_index           pass shapes=35 unindexed=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Measure the calibration's coverage, not just its result.** 138 cases pass, and
the question cycle 1414 raised is untouched: they predate `alias on`, the
instruction overrides and `dump`, and a case set that exercises none of those
cannot fail when one of them breaks. The cheap form is a census — which
directives the 138 specs actually use — and it is a scan of files already in the
workdir.
