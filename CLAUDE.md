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
