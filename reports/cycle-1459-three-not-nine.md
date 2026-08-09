# Cycle 1459 — three, not nine

## Qualification

- **No Ghidra run and no oracle pass.** The spec corpus, the harness source, and
  the twenty-six live micro-execution suites.
- No product C++ changed; ctest stays **56**. **No contract entry.**
- `tools/audit_microexec_calibration_coverage.py` extended to three tiers;
  `CLAUDE.md` corrected.

## The cycle I set out to run, and why it cannot be run

Cycle 1458 said: add a calibration case for `override`, because its expected
value is already derived.

**It cannot be added.** `audit_microexec_harness_calibration.py` does not own a
spec corpus — it *derives* its 138 specs from the committed
`analysis/microexec/**/*.ppc.json` snapshots, which
`MicroExecuteScenarioParser.java`, the **specialised** harness, produced. The
calibration's whole value is that two independent harnesses agree. A case
exercising `override` cannot exist there, because the specialised harness has no
`override` and never will.

So the frozen corpus is closed by construction. Cycle 1458 proposed extending it
without checking what it was.

## Correcting cycle 1458's count

That cycle wrote "nine directives are untested" and built a tool that says so.
Nine are untested **by the frozen corpus**. Counting the live suites — which
cycle 1458 named in its own *not established* section and then did not count:

```
calibrated (frozen corpus, two harnesses agree) : case function gpr region sp stub
exercised only by live suites (port vs harness) : alias capture fpr override steps vec
exercised NOWHERE                               : dump hint vmx
```

**Three, not nine.** `audit_vmx128_behaviours.py` alone exercises `alias`,
`capture`, `override` and `vec`; `audit_transform_kernel_microexec.py` exercises
`fpr` and `override`; `audit_flight_orientation_microexec.py` exercises `steps`.

## The tiers are not a technicality

A **calibration** has two independent producers, so a change in either shows. A
**live suite** compares the port against the harness — so a harness regression
moves both sides and cancels. `override` is the exception that proves the rule:
`audit_transform_kernel_microexec.py`'s expectation is the ported trigonometry
and `fctid`'s correct answer is IEEE rounding, neither of which comes from the
harness.

The tool prints the tiers separately rather than adding them up, because
summing them is exactly the mistake that makes "138/138" sound like "the harness
is fine".

## What is left, and it is the sharp one

`dump` is exercised nowhere — and `dump`'s `region_dumps` key is what made the
comparator report **0 of 138 for eighty-seven commits**. The directive implicated
in this campaign's longest silent failure is still the one nothing runs.

`hint` and `vmx` likewise.

## Not established

- Whether `dump`, `hint` and `vmx` work at all today. Three directives, no
  caller, no test.
- Whether the six "exercised" directives would survive a harness regression.
  Only `override` has an expectation independent of the harness; the rest would
  cancel.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
microexec_calibration_coverage          pass calibrated=6 exercised=6 nowhere=3
claude_md_numbers                       pass checked=3 mismatched=0
instrument_discipline_index             pass shapes=35 unindexed=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Give `dump` a caller.** It is one of three directives nothing runs, it is the
one already implicated in an eighty-seven-commit failure, and the cheapest form
is not a new corpus but a `dump` added to a suite that already runs — the
region it dumps can be asserted against the `memory_writes` the same case
already produces, which makes the expectation internal to a single run and
independent of any port.
