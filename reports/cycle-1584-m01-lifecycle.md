# Cycle 1584 — M01 lifecycle controls

Status: `provisional-covered` for the M01-D pause/restart slice; M01/JV/JP
remain open.

## Contract

`RetailSession` now owns the controller lifecycle boundary for the qualified
runtime. A rising XInput Start bit (`0x0010`) toggles the gameplay HSM between
`Gameplay` and `Paused`; simulation, combat and the retail script cursor do not
advance while paused. A rising Back bit (`0x0020`) restores a checkpoint captured
immediately after the retail entry step. The checkpoint includes flight,
scenario/objectives, combat, unit records, radio, script cursor and sequencer.

The launch checkpoint is allowed at runtime tick zero. This is a valid restart
state, not a fabricated input or a second script driver. `ExternalProbe` remains
diagnostic; the button path is only part of `QualifiedRuntime` product sessions.

No retail container or tracker/tracking/telemetry payload was added.

## Validation

* `cmake --build reconstruction/ace-combat-6/build -j16` — PASS.
* `ac6-retail-session-tests` with the PAL M01 FHM payload — PASS; includes
  rising-edge pause/resume, held-button no-repeat, tick freeze, direct restart,
  Back restart and launch cursor assertions.
* CTest product/combat/session-save/replay tests — PASS (4/4).
* CTest `ac6-retail-replay-trace-cadence` — PASS.

## Boundaries

This slice does not claim a frontend title→briefing→hangar route, campaign
promotion, radio/XMA playback, or direct DrawPacket→swapchain submission. Those
remain M01-D/E and JV/JP work.
