# Cycle 1270 — a false line in my own commit message

## Qualification

No product code changed. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.**

## The error

Commit `29a27a47` — *"Hold cycle 1146's lesson with a control instead of a
comment"* — ends with:

> Contract evidence refreshed for the test file in both contracts.

**That is false.** `reconstruction/ace-combat-6/tests/retail_session_tests.cpp`
is cited by **neither** contract:

```
$ grep -c "tests/retail_session_tests.cpp" analysis/contracts/*.json
mission01-final-gate-v3.json:0
mission01-visible-gate-v4.json:0
```

and no contract file changed in that commit (`git diff HEAD~1 --stat --
analysis/contracts/` is empty).

## How it happened, which is the part worth keeping

The refresher script was pointed at two paths — the test file and
`reports/mission01-retail/retail-session.json` — and reported:

```
mission01-final-gate-v3.json: 2
mission01-visible-gate-v4.json: 2
```

**I read "2" as "the test file was refreshed."** The two are the two citations of
`retail-session.json`, whose content had not changed, so the rewrite was a
no-op. The script counts *sites visited*, not *values altered*, and it says so
in neither its name nor its output.

The tell was there and I walked past it: `git status` after the refresh listed
only the test file, and no contract. A refresh that changed a contract would
have shown the contract as modified. **The instrument's output and the working
tree disagreed, and I believed the instrument.**

## The correction, and why it is a commit rather than an amend

The commit is local and unpushed, so `git commit --amend` would have erased the
line at no cost. That is the wrong move here. The standard this campaign keeps
is that corrections are **recorded**, not removed — a repository whose history
contains no wrong sentences is not a repository that never wrote any.

The claim was also harmless: nothing depended on it, no gate consulted it, and
the control the commit actually added is real and was positively controlled.
**That is precisely why it needed correcting.** A false line beside a true result
is the kind that survives, because everything around it checks out.

## What is true about that commit

- The far-plane control exists, renders the same frame twice differing only in
  the far plane, and requires the derived one to draw strictly more.
- Its positive control was run: forcing both renders to the derived plane turns
  the test red, restoring it green.
- The placed extent is asserted above 10,000 so a collapsed world cannot leave
  the control passing on a one-unit span.
- **No contract cites the test file, and none cites `src/commands.cpp` either.**
  Both changes of the last two cycles sit outside every behaviour's evidence.

## The rule this adds

**A script that reports how many sites it visited has not told you how many it
changed.** When the two can differ, print both, or check the working tree
instead of the count. The refresher used repeatedly this session reports the
first and was read as the second, three times, and only this once did the
difference matter.
