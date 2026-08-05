# Cycle 1006 — bridge lane reaches type28=8 and type28=10

Date: 2026-08-05

## A/B provenance

- Classification: `bridge`; interventions are enabled by the build lane.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6` (no Ghidra export
  was changed in this cycle).
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Bridge executable SHA-256:
  `896b0b79608235d9063231fc472828d8b1c2545438c8dfe5195702174922d055`
- Bridge source worktree commit: `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`
  (dirty external worktree; the local native repository was not used to build
  this ELF).
- Follow log SHA-256:
  `8508fafac19b5aa1fefcebc0c58ef1084f41fb82206f0512aceb30311096a15d`
- Build lane: `AC6_EXPERIMENT_LANE=bridge`.
- Output: `/tmp/ac6-cycle1006-bridge-lane` (full logs not committed).

The matching `stock`-configured ELF from cycles 1004–1005 reached only
`state40=8 / selector44=4 / type28=6` with the same fresh-profile recipe.

## Observed transition

The bridge lane reached:

`type28=6 / selector44=4 → selector44=5 → selector44=6 → selector44=7 /
type28=8 → selector44=8 / type28=10`.

The first exact `type28=8` line is followed immediately by `type28=10`; the
run emitted 4666 `PRESENT` records and no fatal, crash, segmentation, assert,
SIGBUS, or SIGSEGV marker. The process was interrupted after the two captures,
so this is a positive launch-dialog transition, not a complete gameplay run.

The retained captures are bounded and show the transition screens:

- `reports/captures/cycle-1006-bridge-type8.png`, SHA-256
  `8f5f2f103dc5f9877deecf0ab7ec47f98fe81b26a9962423c4523452d1e67c36`
- `reports/captures/cycle-1006-bridge-type10.png`, SHA-256
  `f161f73ede2fa84356797f5ba9d46a4d0c06993da201eee73c7747bc601eb211`

## Boundary

The first reproducible A/B difference is therefore the save/create-to-launch
transition: stock lane `type28=6` stalls, bridge lane reaches `8→10`. This
does not yet prove `Loading→Game`, visible terrain/aircraft/HUD, controls,
audio, or the first black-render rupture. The next recipe continues from
`type28=10` through campaign setup and flight using the same bridge binary.
