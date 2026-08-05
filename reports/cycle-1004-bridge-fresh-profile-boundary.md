# Cycle 1004 — bridge fresh-profile Mission 1 boundary

Date: 2026-08-05

## Provenance

- Classification: `bridge` (recompiled ELF); not `stock/observe` parity and not
  native-runtime evidence.
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Recompiled executable SHA-256:
  `ce5d7d0b5ad4e7d61c7024d886daaa89cdca699e524accf19566f44d44b55e84`
- Follow log SHA-256:
  `02e7d5567795cea43ad092dcc522c50fea8d18c67c08af09de0a454af14e4155`
- Harness output: `/tmp/ac6-cycle1004-bridge-fresh`
- Command:
  `SDL_AUDIODRIVER=dummy tools/ac6-run.sh --out /tmp/ac6-cycle1004-bridge-fresh
  --duration 300 --display :97 --capture-at 0 --startup-timeout 120
  --keys "0:Escape:0.1,2:space:0.1" --wait-for type28=30
  --wait-pulse Escape+space:0.1:2 --wait-stall-timeout 60
  --step-file scripts/ac6-first-mission-fresh-loadout.steps -- ...`

The runtime log says `lane=stock interventions=none`, but it also reports
`graphics mode=hybrid_backend_fixes` and the executable is the recompiled ELF;
the run is therefore retained only as `bridge` evidence.

## Boundary

The fresh-profile, state-synchronised route reached:

`type28=30 → type28=37 → type28=35 / selector44=3 → selector44=10 /
type28=9 → state40=9 / type28=5 → state40=6 / type28=5 → state40=8 /
selector44=4 / type28=6`.

The harness then pulsed `space` at the `type28=6` boundary until its owned
300-second process bound elapsed. No exact `type28=8` or `type28=10` line was
observed. The harness returned 68 because the wait-pulse saw its bounded game
process exit; the runtime logs contain no `fatal`, `crash`, `segmentation`,
`assert`, `SIGBUS`, or `SIGSEGV` marker. The run emitted 18124 `PRESENT`
records.

The retained 1280×720 capture is the slot/create boundary, visually a dark
blue RexGlue screen rather than a Mission 1 world frame:

- path: `reports/captures/cycle-1004-bridge-slot-boundary.png`
- SHA-256:
  `22afef75543b1cc25967cbb4db95b97538ca79d2d8fd0d44afc2e28024f521e1`
- size: 77140 bytes

## Conclusion

This run makes the navigation boundary reproducible with a fresh profile but
does not prove `Loading→Game`, `CModeTaskGame`, visible terrain/aircraft/HUD,
input response, or a renderer rupture. The next discriminant is the qualified
`type28=6 → type28=8` input/save transition; no Xenos correction is inferred.
