# Cycle 1460 — the line that was missing

## Qualification

- **A Ghidra run: yes** — three `dump` cases, then the 138-case calibration again
  after the harness changed, both `analyzeHeadless`, read-only. **No oracle.**
- `scripts/MicroExecuteFunction.java` **changed**; calibration re-run in this
  cycle, as `CLAUDE.md` requires. ctest stays **56**. **No contract entry.**
- New: `tools/audit_microexec_dump_directive.py`.

## A real defect, found by the first thing that checked `dump`

Three cases, each asking for `dump record` and `dump buffer`, produced **2, 4 and
6** dumps respectively — each case re-emitting every earlier case's region names.

`resetCase()` clears twenty-two fields. It did not clear `dumpRegions`:

```java
        assertedFired.clear();
        overrides.clear();
+       dumpRegions.clear();
    }
```

**One missing line, in a list nothing had ever inspected.**

### The blast radius, stated rather than assumed

Every dump entry is computed from the *current* case's regions —
`regions.get(name)` after `resetCase()` has refilled them, and
`passA.get(region.name)` from the current emulation. So an accumulated name
produces a **duplicate of the correct bytes**, not stale bytes from an earlier
case. Suites that read a dump by name take the first match and get the right one.

So: **no published result is wrong.** The output was bloated, and it would have
become wrong the moment two cases in one batch used different region names for
the same slot. That is the defect worth fixing and it is not the defect worth
alarming about, and the difference is a paragraph of reading rather than a guess.

The calibration was re-run after the change: `cases=138 equal=138 failures=0`.

## What the new checker actually checks

`dump` emits both poison passes beside the `memory_writes` the same run derives
from comparing them, so the two are cross-checkable without a port or a second
harness:

1. every reported write appears in the dump, in **both** passes;
2. every byte outside a reported write is still poison — `0xCD` in A, `0x00` in
   B — so a byte that moved unreported is caught;
3. the two passes **agree** wherever a byte was written. Not a tautology: write
   detection fires when *either* pass differs from its poison, so a byte copied
   out of uninitialised memory is reported as written and differs between passes.

```
dump_directive=pass dumps=6 failures=0
```

## Correcting cycles 1458 and 1459, which both published a wrong count

Cycle 1458 said nine directives were exercised nowhere. Cycle 1459 corrected it
to three and named `dump` as the sharpest. **Both figures were artefacts of the
detector**, which matched a directive only where it followed an opening quote —
so it never saw a template-style spec, and `dump` is used by
`audit_container_index_microexec.py`, `audit_control_blend_microexec.py`,
`audit_flight_command_microexec.py`, `audit_flight_controls_microexec.py` and
`audit_flight_step_driver_microexec.py`, all along.

Broadening the regex to line-leading tokens then matched `dump = next(...)`,
ordinary Python. Two wrong detectors in opposite directions, so the third parses
the file and looks **only inside string constants**. It also skips itself, whose
docstring names every directive.

```
calibrated : case function gpr region sp stub
exercised  : alias capture dump fpr hint override steps vec vmx
NOWHERE    : (none)
```

**Zero, not nine, not three.** `hint` and `vmx` are exercised by the three input
suites; `vec` by `audit_vmx128_behaviours.py`; `override` by three suites.

The premise of cycles 1458–1460 — "`dump` has no caller" — was false, and the
cycle built on it still found a real bug, because it did the one thing the
premise implied nobody had done: look at what `dump` produces.

## Not established

- Whether the nine "exercised" directives would survive a harness regression.
  Unchanged: a live suite compares port against harness and cancels.
- Whether any other per-case field is missed by `resetCase()`. Twenty-two are
  cleared and one was not; I read the list and found no second omission, which
  is a reading and not a proof.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
microexec_harness_calibration           pass cases=138 equal=138 failures=0
dump_directive                          pass dumps=6 failures=0
microexec_calibration_coverage          pass calibrated=6 exercised=9 nowhere=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Read `resetCase()` against the field list, mechanically.** One omission was
found by accident and the second-order question is whether there is another: the
class declares its per-case state as fields, `resetCase()` clears a subset, and
comparing the two is a parse rather than a reading. It is the same shape as the
directive census, and the census has now been wrong twice by being done by
pattern instead of by parse.
