# Cycle 1007 — bridge route reaches Mission 01 hangar boundary

Date: 2026-08-05

## Provenance

- Classification: `bridge`; the build lane explicitly enables the bridge
  intervention and is not stock/observe parity evidence.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6` (no Ghidra export
  was changed in this cycle).
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Bridge executable SHA-256:
  `896b0b79608235d9063231fc472828d8b1c2545438c8dfe5195702174922d055`
- Bridge source worktree commit:
  `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11` (dirty external worktree).
- Follow log SHA-256:
  `b2a981ed8290e5e73482287ac2bb056234adc958916cd7235352d6850652e5de`
- Recipe: `scripts/ac6-first-mission-bridge-full-flight.steps`.
- Audio/display: `SDL_AUDIODRIVER=dummy`, Xvfb display `:100`.

## Route and result

The fresh-profile route reached the bridge launch transition

`type28=30 → 37 → 35 → selector44=3/10 → type28=9 → 5 → 6`

then

`selector44=5 → 6 → 7 → type28=8 → selector44=8/type28=10`.

The recipe continued through campaign setup, the campaign intro transition,
Mission 01 selection, weapon confirmation, the mission submenu, and the
deploy confirmation. The follow log contains 15,034 `PRESENT` records, one
exact `type28=8`, three exact `type28=10`, and no `CModeTaskGame` marker. The
process was interrupted after the bounded captures (exit 130 from the owned
Ctrl-C); no fatal, crash, segmentation, assert, SIGBUS, or SIGSEGV marker was
found.

The retained captures show:

- `reports/captures/cycle-1007-bridge-flight-candidate.png` — Mission 01
  aircraft/weapon selection with F-16C and XMA4 visible.
- `reports/captures/cycle-1007-bridge-hud-baseline.png` — visible F-16C hangar
  composition with `MISSION 01 / INVASION OF GRACEMERIA`, `>> STANDBY`, and
  deploy confirmation.
- `reports/captures/cycle-1007-bridge-flight-pitch.png` — SHA-256
  `bb4595b0182a59c06c05fd062be85f768a2f57e473034942d7f16f0359306736`.
- `reports/captures/cycle-1007-bridge-flight-roll.png` — SHA-256
  `f1d52dfc531759e103d5c8d429dd271ddba98a508b655db6b6fdcd623b1f8f19`.
- `reports/captures/cycle-1007-bridge-flight-yaw.png` — SHA-256
  `036b192c9894d7be4ed6307b5ff02e3dcca3681386c9be63285c3d1a965329ec`.
- `reports/captures/cycle-1007-bridge-flight-throttle.png` — SHA-256
  `79d81e29e507825c81440bce66e64dca2115bb76b785e553000f826bca8b2abf`.

The four input captures remain at the hangar/deploy boundary, although the
aircraft/camera composition changes. This is evidence that the bridge lane
can render the campaign and aircraft UI, and it disproves an all-output-black
claim for this route. It is not evidence of airborne `Loading→Game`, terrain,
depth, HUD-in-flight, control parity, audio parity, or a Xenos fix.

## Boundary and next gate

The remaining deterministic boundary is the deploy-confirmation input. The
next recipe must enter airborne `CModeTaskGame`, capture a real flight HUD and
world frame, and only then trace `clear → targets → draws → resolve → present`.
Stock/observe remains unqualified beyond the earlier `type28=6` stall; this
bridge result cannot be promoted to stock evidence.
