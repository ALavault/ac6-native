---
name: ac6-regression-recovery
description: Recover a known-good AC6 state and isolate regressions with Git.
---

# AC6 regression recovery

Use this skill when a previously working AC6 demo behavior regresses. Recover
the smallest proven change; do not merge an old branch wholesale.

1. Define the symptom, the last known-good witness, and the current failing
   witness. Use the PAL demo oracle and codegen ON for both sides unless the
   gate explicitly tests another variable. Preserve visual captures when the
   symptom is graphical.
2. Inspect `git status`, relevant history, and diffs. Use an isolated
   `git worktree` for candidate revisions. Use `git bisect` only when the
   gate is deterministic and its good/bad criterion is explicit.
3. Rebuild and run the smallest qualified gate at each candidate. Keep the
   input, runtime configuration, capture window, and validation criterion
   fixed; do not introduce an A/B unless one named causal variable requires it.
4. Compare the minimal relevant diff and evidence. Apply or cherry-pick only
   the focused fix after it reproduces the known-good witness. Never use
   `git reset --hard`, broad checkout, or an unreviewed merge to recover state.
5. Store bisect notes, candidate results, and captures under
   `artifacts/<gate>/`; update `STATE.md`, `EVIDENCE.md`, `NEXT.md`, and
   `RESUME.md` with the regression cause and remaining boundary.

Keep demo and retail projects, binaries, and captures separate. Treat codegen
OFF as a diagnostic comparison, not as the visual or behavioral reference.
If no deterministic good/bad gate exists, stop with a named blocker instead of
calling a plausible historical commit a fix.
