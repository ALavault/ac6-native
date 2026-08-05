# Cycle 1002 — bridge Mission 1 route boundary

Date: 2026-08-05

## Provenance

- Classification: `bridge` (recompiled ELF observation); this is not
  `stock/observe` evidence and is not native-runtime reproduction.
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Recompiled executable SHA-256:
  `ce5d7d0b5ad4e7d61c7024d886daaa89cdca699e524accf19566f44d44b55e84`
- Harness: `scripts/run_ac6_first_mission.sh`
- Command: `SDL_AUDIODRIVER=dummy scripts/run_ac6_first_mission.sh
  --out /tmp/ac6-cycle1002-bridge90 --display :96 --duration 90
  --attempts 1`
- Output logs remain in `/tmp/ac6-cycle1002-bridge90/attempt-1` and are not
  committed.

The application log declares `lane=stock interventions=none`, but the same
run reports `graphics mode=hybrid_backend_fixes` and uses the recompiled ELF;
the evidence is therefore recorded as `bridge`.

## Observed route

The scripted run reached these states in order:

1. `type28=30`, `state40=6`, `selector44=0`;
2. `type28=37`;
3. `type28=35`;
4. `selector44=3`;
5. `type28=6`.

The harness timed out waiting for `type28=8`. The run emitted 3331
`PRESENT` records. A bounded search of both runtime logs found no
`fatal`, `crash`, `segmentation`, or `assert` line.

The retained 1280×720 `t0` capture is a Bandai Namco splash, not a Mission 1
world frame:

- path: `reports/captures/cycle-1002-bridge-t0.png`
- SHA-256:
  `f8ea0650fc8de5df7630a7e750753fcb5440942c143fff5f1ae50f0a18587203`
- size: 24966 bytes

## Boundary and next gate

This run proves bridge startup and partial menu/mission routing only. It does
not prove `Loading→Game`, visible terrain/aircraft/HUD, input response, or the
first black-render rupture. The next evidence gate is a qualified
`type28=8`/gameplay capture followed by a stock-versus-bridge A/B; no renderer
correction is inferred from this run.
