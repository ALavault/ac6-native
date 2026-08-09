#!/usr/bin/env python3
"""Re-measure the numbers `CLAUDE.md` cites, because two of them were wrong.

`CLAUDE.md` is read at the start of every cycle and its figures are used the way
a constant is used: quoted, not re-derived. That is exactly the condition under
which a number rots.

Two had:

  - **"138 of 138"** for the harness calibration. It was **0 of 138 for
    eighty-seven commits** while the file said otherwise; cycle 1414 fixed the
    comparator rather than the claim, and nothing would have caught it but a
    cycle that happened to run the calibration for its own reasons.
  - **"`exports/` recovers 6"** of `0x822A23D8`. The export carries p-code for
    **two** instruction addresses. Whether the six was wrong when written or the
    export was regenerated afterwards cannot be told from here, which is the
    point: an unmeasured number gives you no way to find out.

A third was nearly added to that list and was not: cycle 1455 wrote that
`CLAUDE.md` cites `INSTRUMENT_DISCIPLINE.md`'s shape count. It does not -- that
figure came from a session summary. The file's shape count was still wrong (31
against 35) and cycle 1455 still fixed the checker; only the attribution was
invented, and this tool exists partly so that attribution is never needed again.

So this exists. Every claim below is re-measured from the image, the repository
or another checker, and a mismatch is a failure rather than a remark.

Not everything is checkable here. The harness calibration needs an
`analyzeHeadless` pass, so it is listed as UNCHECKED rather than silently
omitted -- an audit that hides what it cannot reach is the shape it exists to
prevent.

usage: audit_claude_md_numbers.py [CLAUDE.md]
exit 0 when every checkable claim holds, 1 otherwise.
"""

import json
import os
import re
import subprocess
import sys

IMAGE = "analysis-input/ACE6_X360.exe"


def run(args):
    out = subprocess.run([sys.executable] + args, capture_output=True, text=True)
    return out.stdout + out.stderr


def pdata_instructions(addr):
    text = run(["tools/check_listing_against_pdata.py", IMAGE, addr])
    m = re.search(r"declared (\d+) instructions \((0x[0-9A-Fa-f]+)\.\.(0x[0-9A-Fa-f]+)\)", text)
    return (int(m.group(1)), m.group(2), m.group(3)) if m else (None, None, None)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "CLAUDE.md"
    text = open(path).read()
    failures = []
    checked = 0

    def claim(name, expected, actual):
        nonlocal checked
        checked += 1
        ok = expected == actual
        print("  %-46s cited %-8s measured %-8s %s"
              % (name, expected, actual, "ok" if ok else "MISMATCH"))
        if not ok:
            failures.append(name)

    def cited_int(pattern):
        m = re.search(pattern, text)
        return int(m.group(1)) if m else None

    # 0x822A23D8: the length, and what exports/ recovers of it.
    count, lo, hi = pdata_instructions("0x822A23D8")
    claim("0x822A23D8 instructions", cited_int(r"`0x822A23D8` is (\d+) instructions"), count)
    recovered = None
    if os.path.exists("exports/822a23d8.json"):
        blob = json.load(open("exports/822a23d8.json"))
        recovered = len({e["address"] for e in (blob.get("pcode") or [])})
    claim("exports/ recovers of 0x822A23D8",
          cited_int(r"`exports/` recovers (\d+)"), recovered)

    # 0x82263A50's indirect branches, over the extent .pdata declares -- NOT a
    # window. Guessing the range gives bctr=2 and the wrong verdict; cycle 1456
    # made that mistake inside the cycle that wrote this.
    _, lo, hi = pdata_instructions("0x82263A50")
    bctr = None
    if lo:
        out = run(["tools/count_indirect_branches.py", IMAGE, lo, hi])
        m = re.search(r"bctr=(\d+)", out)
        bctr = int(m.group(1)) if m else None
    claim("0x82263A50 bctr", _word_to_int(text, r"`0x82263A50` has (\w+) `bctr`"), bctr)

    # The calibration cannot run from here, but WHEN IT LAST RAN can be read,
    # and a stale date is the failure mode: it was 0 of 138 for 87 commits with
    # nothing in the tree to say so.
    m = re.search(r"Last actually run: cycle (\d+) — `cases=(\d+) equal=(\d+)", text)
    if m:
        print("  %-46s last run cycle %s, %s of %s -- rerun after touching "
              "MicroExecuteFunction.java" % ("harness calibration", m.group(1),
                                             m.group(3), m.group(2)))
    else:
        print("  %-46s %s" % ("harness calibration",
                              "UNCHECKED and no run recorded -- see CLAUDE.md"))
        failures.append("harness calibration has no recorded run")

    status = "pass" if not failures else "fail"
    print("claude_md_numbers=%s checked=%d mismatched=%d" % (status, checked, len(failures)))
    return 0 if not failures else 1


WORDS = {"one": 1, "two": 2, "three": 3, "four": 4, "five": 5, "six": 6,
         "seven": 7, "eight": 8, "nine": 9, "ten": 10}


def _word_to_int(text, pattern):
    m = re.search(pattern, text)
    if not m:
        return None
    token = m.group(1)
    return WORDS.get(token.lower(), int(token) if token.isdigit() else None)


if __name__ == "__main__":
    sys.exit(main())
