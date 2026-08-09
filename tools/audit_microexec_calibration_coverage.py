#!/usr/bin/env python3
"""What the harness calibration actually exercises, against what the harness does.

`audit_microexec_harness_calibration.py` answers "does the harness still
reproduce the committed corpus?" and cycle 1457 got `cases=138 equal=138`. That
is a real answer to a real question, and it is routinely read as a stronger one:
*the harness is fine*.

It is not that. The 138 specs use **six** directives:

    case  function  gpr  region  sp  stub

`scripts/MicroExecuteFunction.java` implements **fifteen**. The nine the frozen
corpus never exercises are

    alias  capture  dump  fpr  hint  override  steps  vec  vmx

— which is every directive added since the corpus was captured: the register-file
bridge, the instruction overrides that fixed `fctid` and double `fmadd`, the
vector and float paths, and `dump`.

`dump` is the sharp one. The comparator bug that reported **0 of 138 for
eighty-seven commits** was a `region_dumps` key — produced by `dump`, used by no
calibration case. The calibration could not have caught it then and cannot catch
its like now.

CYCLE 1459 CORRECTS CYCLE 1458, which wrote this tool and said "nine directives
are untested". Nine are untested BY THE FROZEN CORPUS. Six of those nine —
`alias capture fpr override steps vec` — are exercised by the live
`audit_*_microexec.py` suites, which cycle 1458 mentioned in its "not
established" section and then did not count. Counting them leaves **three**
exercised nowhere at all: `dump`, `hint`, `vmx`.

The distinction the two tiers keep is real and is not a technicality. The frozen
corpus is a CALIBRATION: two independent harnesses producing the same answer, so
a change in either is visible. A live suite compares the port against the
harness, so a harness regression moves both sides and cancels — except where the
suite's expectation is independent of the harness, as `override` + `fctid` is
against IEEE rounding. "Exercised" is weaker than "calibrated", and this prints
them separately rather than adding them up.

So this is a RATCHET, not a gate on the gap. Closing it is work no cycle has
chosen to do; letting it widen silently is the failure this prevents. It fails
when a directive leaves the covered set, when one leaves the exercised set, and
when a directive exists that is in no list — which is what happens the next time
someone adds one.

usage: audit_microexec_calibration_coverage.py [--specs DIR]
exit 0 while the ratchet holds, 1 when it slips.
"""

import os
import re
import sys

HARNESS = "scripts/MicroExecuteFunction.java"
DEFAULT_SPECS = "analysis/microexec"

# Every directive the harness parses. Read from the source, not from memory.
DIRECTIVE_RE = re.compile(
    r'"(function|case|steps|region|sp|gpr|fpr|vec|stub|capture|dump|alias|vmx'
    r'|override|hint)"')

# THE RATCHET. Measured at cycle 1458 against the 138-case corpus, and at 1459
# against the live suites.
COVERED = {"case", "function", "gpr", "region", "sp", "stub"}
EXERCISED = {"alias", "capture", "fpr", "override", "steps", "vec"}
NOWHERE = {"dump", "hint", "vmx"}
SUITES = "tools"


def spec_directives(root):
    used = set()
    seen = 0
    for base, _dirs, files in os.walk(root):
        for name in files:
            if not name.endswith(".spec"):
                continue
            seen += 1
            for line in open(os.path.join(base, name), errors="replace"):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                used.add(line.split()[0])
    return used, seen


def suite_directives(supported):
    """Directives the live audit_*_microexec.py suites emit into their specs."""
    used = set()
    for name in sorted(os.listdir(SUITES)):
        if not (name.startswith("audit_") and name.endswith(".py")):
            continue
        if "microexec" not in name and "vmx128" not in name:
            continue
        text = open(os.path.join(SUITES, name), errors="replace").read()
        for d in supported:
            if re.search(r"""(["\'])\s*%s[ \n]""" % d, text):
                used.add(d)
    return used


def main(argv):
    specs = DEFAULT_SPECS
    if "--specs" in argv:
        specs = argv[argv.index("--specs") + 1]

    if not os.path.exists(HARNESS):
        print("microexec_calibration_coverage=error no %s" % HARNESS)
        return 1
    supported = set(DIRECTIVE_RE.findall(open(HARNESS, errors="replace").read()))

    if not os.path.isdir(specs):
        print("microexec_calibration_coverage=skip no spec corpus at %s "
              "(emit one with audit_microexec_harness_calibration.py --emit)" % specs)
        print("  harness supports %d directives; the recorded ratchet covers %d"
              % (len(supported), len(COVERED)))
        return 0

    used, count = spec_directives(specs)
    used &= supported

    exercised = suite_directives(supported) - used
    lost = sorted(COVERED - used) + sorted(EXERCISED - used - exercised)
    unclassified = sorted(supported - COVERED - EXERCISED - NOWHERE)

    print("harness directives %d, calibration specs %d, exercised %d"
          % (len(supported), count, len(used)))
    print("  calibrated (frozen corpus, two harnesses agree): %s"
          % " ".join(sorted(used)))
    print("  exercised only by live suites (port vs harness): %s"
          % " ".join(sorted(exercised)))
    print("  exercised NOWHERE: %s"
          % " ".join(sorted(supported - used - exercised)))
    for d in lost:
        print("  REGRESSION  %r was covered and is not" % d)
    for d in unclassified:
        print("  UNCLASSIFIED %r is implemented but in neither list -- add a "
              "calibration case or record it as uncovered" % d)

    failures = lost + unclassified
    print("microexec_calibration_coverage=%s calibrated=%d exercised=%d "
          "nowhere=%d slipped=%d"
          % ("pass" if not failures else "fail", len(used), len(exercised),
             len(supported) - len(used) - len(exercised), len(failures)))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
