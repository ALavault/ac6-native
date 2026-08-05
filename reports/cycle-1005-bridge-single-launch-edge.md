# Cycle 1005 — bridge single launch edge negative

Date: 2026-08-05

## Provenance

- Classification: `bridge`; no stock parity or native-runtime claim.
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Recompiled executable SHA-256:
  `ce5d7d0b5ad4e7d61c7024d886daaa89cdca699e524accf19566f44d44b55e84`
- Follow log SHA-256:
  `d3fa8af4bbef5873fb1c0cb4dc777bbcefa545bd541a2e1d6a9378e10e32415a`
- Recipe: `scripts/ac6-first-mission-fresh-launch-probe.steps`
- Output logs: `/tmp/ac6-cycle1005-bridge-single-edge` (not committed).

## Result

The fresh-profile recipe reaches the same route as cycle 1004 and then sends
exactly one `Left` followed by one `space` at `type28=6`:

`type28=30 → 37 → 35 / selector44=3 → selector44=10 / type28=9 →
state40=9 / type28=5 → state40=6 / type28=5 → state40=8 /
selector44=4 / type28=6`.

The recipe then pulses `space` until the bounded 180-second run ends. It emits
10920 `PRESENT` records, but no exact `type28=8` or `type28=10` record. A
bounded search of both runtime logs finds no `fatal`, `crash`, `segmentation`,
`assert`, `SIGBUS`, or `SIGSEGV` marker. The harness returns 68 because its
wait-pulse observes the owned process ending at the duration bound.

The boundary capture is byte-identical to cycle 1004 and is retained at:
`reports/captures/cycle-1004-bridge-slot-boundary.png` (SHA-256
`22afef75543b1cc25967cbb4db95b97538ca79d2d8fd0d44afc2e28024f521e1`). It is
not a gameplay frame.

## Boundary

The extra `Left` in the prior fresh-profile recipe is not the cause of the
stall. The remaining qualified question is the save/create or launch contract
after `type28=6`; no renderer correction is inferred and no guest state is
forced by this experiment.
