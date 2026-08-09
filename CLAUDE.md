# Working mode for this repository

## Autonomy

Work autonomously. Do not stop to report progress, ask permission for an
ordinary technical choice, or seek confirmation that a direction is right.

Interrupt only for:

- a **qualified blocker** — work that cannot proceed without something outside
  the repository (an oracle session and its N3 budget, a retail archive that is
  not in the workspace, a decision that spends a resource the milestone
  reserved);
- an **ambiguity that would materially change the work** — two readings of the
  request that lead to different deliverables.

Everything else is decided and **written down in the cycle report**, not raised
as a question. "I chose X over Y because Z" belongs in the report; it is not a
reason to stop.

A finished piece of work ends with a commit and the next piece starting, not
with a summary addressed to the user.

## What a cycle is

One unit of work, one report in `reports/cycle-NNNN-*.md`, one commit. Every
report carries:

- **Qualification** — which Ghidra project, which XEX SHA-256, which payload,
  and whether an oracle was used (the answer has been "no" for the whole
  campaign);
- what is **established**, with concrete addresses;
- what is **not established**, stated as plainly as what is;
- the **decisions taken** in place of asking.

## Evidence discipline

- Never assert a value that was not read. Constants, vtable slots and struct
  offsets get looked up, not inferred from a name. Cycle 1133 corrected an
  exhaustiveness claim of its own predecessor that had stood ninety minutes;
  cycle 1134 corrected ten cycles that had scanned the wrong offset.
- A plausible rule with no control is refused. Cycles 1111 and 1113 killed two
  such rules, and that is the standard.
- Correct predecessors **and yourself**, in the report, by name and cycle number.
- Measure the instrument before trusting it. Four versions of one scan counted
  stack frames, then memsets, then constant runs, before it found anything.
- Xenia is an oracle only, and the generated XenonRecomp C++ is literal
  cross-match evidence only — never a source for native behaviour.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`
must exit 0, and `ctest` must pass, before any commit. Both are cheap; run them
every time.

Then **`git status` after `ctest`, not before**. The suite regenerates the metrics
artefacts that contracts cite; if one comes back modified, the committed version
described a different run and a fresh clone would disagree with this tree. That
has happened once — a regenerated `ntxr-decode.json` was never staged while its
hash was refreshed against disk — and the artefact checker cannot catch it,
because it compares HEAD against the working tree and both were wrong together.

Two more checkers exist and are as cheap:

    python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
    python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json

The first verifies every cited artefact is in HEAD and matches the tree. The
second verifies every `retail_addresses` entry is actually mentioned by one of
that behaviour's own evidence files — a citation to nowhere is not a derivation.
It caught its author three times on the day it was written.

When a gate fails on `evidence size mismatch`, the fix is to re-pin the hashes,
and there is a tool for it rather than a heredoc:

    python3 tools/refresh_contract_evidence.py analysis/contracts/*.json -- <edited paths>

It prints `sites=N changed=M` per contract and **fails on a path no contract
cites**. Both matter: a heredoc version reported only the sites it visited, and
"2 sites, 0 changed" was read as "the file I edited was refreshed" — for a file
cited by neither contract. Run it after `ctest`, never before, for the reason
above.

The gate stops at its first failure, which is right for a gate and wrong for
fixing one. This maps the whole problem in a single pass:

    python3 tools/audit_contract_derivations.py analysis/contracts/*.json

It names every derivation file that fails to cite all of its behaviour's
`retail_addresses`, and every behaviour carrying **more than one** derivation
file — because the gate requires each of them to cite all addresses
independently, which for a header/implementation pair forces either duplication
or a false claim. Cycle 1261 fixed one address the gate named and found ten;
nine were not missing citations at all.

Before drawing a negative from a listing, check it is the whole function:

    python3 tools/check_listing_against_pdata.py analysis-input/ACE6_X360.exe ADDR --listing FILE

`.pdata` declares each function's length in instructions, so the comparison is
arithmetic. `Ac6XenonDisasm` caps at 300 per block and `exports/` silently
truncates VMX128-heavy functions — `0x822A23D8` is 460 instructions and
`exports/` recovers 6. The table is incomplete, so "no `.pdata` row" is a real
answer; the tool says so rather than guessing.

And before believing you have read a dispatcher:

    python3 tools/count_indirect_branches.py analysis-input/ACE6_X360.exe START END

`0x82263A50` has three `bctr` and was read twice as though it had one.

A capture's committed `.png` files are `pnmtopng` conversions run by hand, and
nothing re-runs them. This connects them to the metrics beside them:

    python3 tools/audit_capture_images_match_metrics.py reports/mission01-native-captures/*/

It reproduces the renderer's FNV-1a colour hash from the PNG and compares it
with the `color_hash` the metrics record. Cycle 1273 shipped a capture whose
images showed 4 markers while its metrics said 29, and the contract pinned both
— correctly, to different states of the same render. Run it whenever a capture
is regenerated.

A third checker guards the documentation instead of the contracts:

    python3 tools/audit_instrument_discipline_index.py INSTRUMENT_DISCIPLINE.md

`INSTRUMENT_DISCIPLINE.md` is read mid-investigation, by scanning its symptom
table — never front to back. A shape that is written but not indexed is, for
that reader, not written, and this catches exactly that. Run it whenever you add
a shape; it is instant.
