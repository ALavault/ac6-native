---
name: ac6-gate-runner
description: Run one falsifiable AC6 reconstruction gate and preserve its evidence.
---

# AC6 gate runner

Use this skill when an AC6 native-recompilation task needs a bounded, auditable
investigation. It is for the qualified PAL demo, not the retail target.

1. Read `reports/handoff/CURRENT.json` and its referenced AC6 report/working
   set, then inspect `STATE.md`, `EVIDENCE.md`, `NEXT.md`, and `RESUME.md`.
2. Write one gate statement with a falsifiable hypothesis and an explicit
   `done_when`. Do not combine unrelated renderer, input, and gameplay gates.
3. Exhaust the canonical static evidence first: the qualified demo Ghidra
   project, XenonAnalyse, XenonRecomp/XenosRecomp, and existing artifacts.
   Generated recompilation output is evidence only and must not be edited.
4. Use runtime only for a named remaining causal ambiguity. Record its exact
   input, observables, bounded frame/time window, and stopping condition before
   launching it. Do not force guest state or use a broad trace by default.
5. Put complete command output and captures under
   `artifacts/<gate>/`. Return only a compact conclusion in `RESULT.md` (or
   `BLOCKER.md` when the gate cannot close). Label claims `PROUVÉ`,
   `FORTEMENT ÉTAYÉ`, `CANDIDAT`, `RÉFUTÉ`, or `INCONNU`.
6. At closure, update the four research handoff files with what changed, what
   was refuted, the exact remaining boundary, and the next discriminating
   experiment. Keep `supported=false` until the final qualification.

Keep demo and retail evidence separate, preserve existing user changes, and
stop when the gate's `done_when` is met. Do not turn a useful observation into
a global invariant without a qualified source and reproducible evidence.
