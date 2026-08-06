# Cycle 682 — D5B4 constant diagnostic: harness boundary

Date: 2026-08-03 (Europe/Paris)

The new off-by-default `ac6_log_d5b4_constants` hook compiles in the bridge
lane and is restricted to unique `(VS hash, D5B4 texture base)` draws. It would
record the raw and IEEE-754 values of pixel constants `c129`, `c138`, `c140`,
`c142`, `c254` and `c255` without changing uniforms or shader translation.

The bounded attempts did not reach a D5B4 draw, so they are not graphics
evidence and must not be compared with cycle 675:

```text
binary SHA-256: f2eb1fa6a569ecfa1feef0e871549622364461876b144392946e2b49b57f450f
cycle-682 fresh route: window/route ended at step 73, before the cutscene
cycle-682b no-constant control: same transition boundary
cycle-682c/d/e launch routes: campaign-transition wait timed out or game exited
all attempts: `[ac6-d5b4-const]` records = 0
```

The runner had a real race: SDL can recreate the game window between a state
predicate and the next key. `tools/ac6-run.sh` now retries focus for at most
10 seconds before failing; a genuinely exited process remains fatal. This
improves reproducibility but does not turn the failed attempts into runtime
evidence.

The current low-oracle decision is to stop here. Do not repeat the full fresh
route unchanged. The next runtime sample should use an existing-save or
scene-window route that begins after the campaign transition and has one
explicit capture gate for the two D5B4 draws. Until that route is qualified,
the static shader contract, cycle 681 sample-zero A/B and the portable
`VulkanMaterialBinding` contract are the accepted boundaries.
