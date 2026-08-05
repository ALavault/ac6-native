# Cycle 1010 — Mission 01 gameplay loop qualified before black-world render

Date: 2026-08-05

## Provenance and recipe

- Classification: `bridge`; this is recompilation evidence with the external
  intervention lane enabled, not stock/observe parity.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`.
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Bridge executable SHA-256:
  `896b0b79608235d9063231fc472828d8b1c2545438c8dfe5195702174922d055`
- External bridge source commit:
  `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11` (dirty worktree; not modified by
  this cycle).
- Follow log SHA-256:
  `e9205757e75185e83e1144029ced9e9be3a4fbe7290202f7528dba7186921937`
- Recipe: `scripts/ac6-first-mission-bridge-airborne-probe.steps`.
- Runtime flags: `--ac6_log_ui_dispatch=true` and
  `--ac6_log_gameplay_state=true`.
- Headless setup: `SDL_AUDIODRIVER=dummy`, Xvfb `:104`, 1280x720.

The log declares `lane=bridge` with
`save-dialog-synthesis,force-cvars,fallback-allocator` and
`graphics mode=hybrid_backend_fixes`. No generated C++ or Ghidra export was
changed.

## Gameplay boundary

The route reaches the qualified sequence

`type28=30 → 37 → 35 → selector44=3 → 10 → type28=8 → 10`

then creates the profile, enters campaign setup, completes Mission 01 loadout,
briefing and cinematic, and reaches the flight HUD. The run emitted 18,841
`PRESENT` records and was stopped after the bounded control captures with the
owned harness exit 130. The follow log contains no fatal, crash, segmentation,
assert, SIGBUS or SIGSEGV marker.

The runtime gameplay probe gives stronger evidence than the image alone:

- `ac6-gameplay-mode-task`: 14 samples at function `0x8219A140`, object
  `0xB8EA0400`, vtable `0x82064384` — the qualified `CModeTaskGame` path.
- `ac6-gameplay-tick`: 11 samples at `0x8226D1C8`, with manager vtable
  `0x8206457C` and HSM vtable `0x82064648`.
- Phases: `UpInput=11`, `UpObj=9`, `UpCam=12`, `UpRadio=9`.
- Player: `0xB2470000`, vtable `0x820568D4`; one live child
  `0xB2470100`, vtable `0x82007A10`; 57 player-update and 40 child-dispatch
  samples.
- Canonical input: 640 samples. The isolated W/S/A edges reach
  `raw_ly=0x7FFF`, `raw_ly=0x8001` and `raw_lx=0x7FFF` respectively, with
  neutral samples afterward.
- The older direct flight hooks at `0x82329B40` and `0x823046A0` execute zero
  times. They are not the player's current owner path and must not be patched
  as if they were.

## Render boundary

The Mission 01 briefing/map capture is visible and the following flight HUD
contains the expected green HUD geometry, but the world is black. The input
captures remain black-world frames; their absolute-pixel deltas against the
HUD baseline are 1,309 (pitch), 64,728 (roll), and 67,651 (yaw), while the
later throttle/brake captures are identical to the yaw capture. These deltas
are presentation/HUD evidence only, not proof of aircraft motion.

The same interval records 55,440 non-null `[ac6-bind]` entries, 1,001
`[ac6-resolve]` entries and 20,960 empty-resolve-region warnings, while
`PRESENT` continues. The warnings are a renderer hypothesis boundary, not yet
the identified first bad stage: this run had `ac6_render_capture=false`, so it
does not provide a per-frame clear/target/draw/resolve catalog.

Retained bounded captures:

- `reports/captures/cycle-1010-bridge-mission-briefing.png` — SHA-256
  `17b82a90f0aa158a3f06167bb7ae3c2d43a047155c84030df186515882e1d605`.
- `reports/captures/cycle-1010-bridge-mission-cinematic.png` — SHA-256
  `a35521c53d67651f386c7093c237bdb69d981cf59ccd431f3a4e1282a444af11`.
- `reports/captures/cycle-1010-bridge-flight-hud-baseline.png` — SHA-256
  `fae095ccf37ade870e8d303a3827b82a1e6dd7448604e6788813cd13c31ad8b1`.
- `reports/captures/cycle-1010-bridge-flight-pitch-up.png` — SHA-256
  `52334c6b780d7e8ede34d160c79f80c99f9a84a8cfedbb578478b38d4613c9f5`.
- `reports/captures/cycle-1010-bridge-flight-roll-left.png` — SHA-256
  `e084370bb4b3760241616f7054c17b6d3e276750ca43d4e6bf615e06cb5dd014`.
- `reports/captures/cycle-1010-bridge-flight-yaw.png` — SHA-256
  `fa782c5f948c3e4310d39f9f1ee0b821cbd34f26f9d7c5f5f267e4cf063e3a14`.
- `reports/captures/cycle-1010-bridge-flight-throttle.png` — same hash as the
  yaw capture (`fa782c5f948c3e4310d39f9f1ee0b821cbd34f26f9d7c5f5f267e4cf063e3a14`).
- `reports/captures/cycle-1010-bridge-flight-brake.png` — same hash as the
  yaw capture (`fa782c5f948c3e4310d39f9f1ee0b821cbd34f26f9d7c5f5f267e4cf063e3a14`).

## Next discriminant

The gameplay/HSM/input chain is now separated from the black-world renderer.
Run the same route with `ac6_render_capture=true` and
`ac6_log_frontier_draws=true`, bounded to the briefing-to-HUD interval, and
publish a compact per-frame catalog of clears, render targets, draws, depth,
resolves and swap source. Only a named divergence in that catalog authorizes a
renderer correction; `PRESENT` or a different black frame is insufficient.
Stock/observe remains open.
