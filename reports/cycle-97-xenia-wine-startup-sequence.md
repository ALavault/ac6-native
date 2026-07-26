# AC6 Xenia through Wine — startup sequence (2026-07-17)

## Scope

This report records the current AC6 retail-oracle launch route after the
Xenia launcher was adapted to run the pinned Windows build through Wine. It is
an evidence boundary, not a claim of mission or flight parity.

Target identity:

- target: AC6 Xbox 360 retail XEX
- emulator route: pinned Xenia Canary Windows release `16e1eb8`
- host route: Wine 64-bit, Vulkan
- launch script: `workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh`
- Xenia SHA-256: `c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`
- input configuration: `hid = "winkey"`, `keyboard_mode = 1`,
  `keybind_start = "0x0D"` (Return), with AZERTY-compatible left-stick
  bindings checked by the launcher preflight

## User-confirmed startup observation

The observed startup order is:

1. developer splash screen;
2. copyright screen;
3. an introductory cinematic;
4. the cinematic can be skipped with `Start`;
5. the title screen.

This observation is attributed to the user-run session. It is not presented
as a replayable trace until a bounded capture records the window state, input
event and resulting frame or emulator state for each transition.

## Local launch gate

The read-only launcher preflight was executed without starting Xenia:

```text
status=ready
release=16e1eb8
renderer=vulkan
service=ac6-xenia-wine-gui.service
```

The check verifies the executable and configuration hashes, the AC6 ground
fix, Vulkan, keyboard mode and the Return/AZERTY bindings. It does not verify
that a mission loads or that a controller action reaches a flight object.

## What this closes

- Xenia can be launched through the Wine route on the configured host.
- The startup horizon and the semantic role of `Start` during the cinematic
  are now known well enough to design a bounded observation recipe.
- A future run should wait for the title-screen predicate before attempting
  any title input; it must not press `Start` blindly during the splash or
  cinematic.

## What remains open

- no persisted Xenia savestate or repeatable state-load contract is qualified;
- no machine-readable frame/state capture has yet been attached to the
  startup transitions;
- title-to-campaign selection and post-CUT mission ownership remain open;
- the existing `POST_CUT_TRANSITION_BLOCKER.md` remains authoritative: do not
  synthesize a flight scene from the title or cinematic sequence.

## Next bounded dynamic action (only on explicit request)

Run the Wine launcher with a fresh isolated profile, record the five startup
states and the single `Start` edge used to skip the cinematic, then stop at the
title screen. Persist only hashes, timestamps, window-state labels and bounded
captures; do not distribute the retail XEX, assets or raw emulator memory.

