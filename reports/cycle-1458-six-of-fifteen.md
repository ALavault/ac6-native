# Cycle 1458 — six of fifteen

## Qualification

- **No Ghidra run and no oracle pass.** The spec corpus cycle 1457 emitted, and
  `scripts/MicroExecuteFunction.java`.
- No product C++ changed; ctest stays **56**. **No contract entry.**
- New: `tools/audit_microexec_calibration_coverage.py`. `CLAUDE.md` records the
  coverage.

## The number is right and narrower than it sounds

Cycle 1457 ran the calibration and got `cases=138 equal=138 failures=0`, then
named the untouched question: the 138 cases predate `alias`, `override` and
`dump`, so a set that exercises none of them cannot fail when one breaks.

Counted, across all 138 specs:

```
exercised   case  function  gpr  region  sp  stub                     6
implemented alias capture case dump fpr function gpr hint override
            region sp steps stub vec vmx                            15
never used  alias capture dump fpr hint override steps vec vmx        9
```

**Six of fifteen.** The nine untested are every directive added since the corpus
was captured: the register-file bridge, the instruction overrides that fixed
`fctid` and double `fmadd`, the vector path, the float path, `capture`, `steps`
and `dump`.

## The sharp one is `dump`

The comparator bug that reported **0 of 138 for eighty-seven commits** was a
`region_dumps` key — produced by `dump`, used by **no calibration case**.

So the calibration could not have caught it, and cannot catch its like now. That
reframes what cycle 1457 established: the harness reproduces the corpus exactly,
on the sixth of its surface the corpus touches.

## A ratchet, not a gate

Closing nine gaps is work no cycle has chosen, and a checker that stays red until
someone does is a checker that gets ignored. So
`audit_microexec_calibration_coverage.py` fails on two things instead:

- a directive **leaving** the covered set;
- a directive implemented but in **neither** list — which is exactly what happens
  the next time one is added without a case.

**Measured before use**, both ways: a corpus stripped of `stub` gives
`REGRESSION 'stub' was covered and is not`, exit 1; a harness with an invented
sixteenth directive gives `UNCLASSIFIED 'trace' ... add a calibration case or
record it as uncovered`, exit 1. It also skips cleanly with no corpus present,
because the corpus is emitted into a scratch workdir and never committed.

## Not established

- Whether the nine uncovered directives currently work. **Nothing here tests
  them.** They are exercised in anger by the twenty-odd `audit_*_microexec.py`
  suites, which is not the same as being calibrated against a frozen corpus —
  those suites compare the port against the harness, so a harness regression
  moves both sides.
- Which of the nine would be cheapest to cover. `steps` and `stub` shapes are
  already in the corpus; `override` and `alias` need a case built around a known
  defect.

## Gates

```
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
microexec_calibration_coverage        pass covered=6 uncovered=9 slipped=0
claude_md_numbers                     pass checked=3 mismatched=0
instrument_discipline_index           pass shapes=35 unindexed=0
tools/tests                           Ran 79 tests, OK
```

## Next

**Add a calibration case for `override`.** It is the directive with a known
right answer — `fctid` truncates in this SLEIGH module and double `fmadd` is
unfused, both established with addresses — so a case is a spec plus the value it
must produce, and it converts a defect this campaign already paid for into a
regression test. One of the nine, chosen because its expected output is already
derived rather than needing a new derivation.
