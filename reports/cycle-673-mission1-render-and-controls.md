# Cycle 673 — Mission 1 renderer and control boundary

Date: 2026-08-03

## Result

The bounded bridge run reached Mission 1 through the qualified fresh route.
The aircraft selection scene is textured, and the overhead introduction
cinematic renders terrain. This closes the white-aircraft defect observed
before the signed-view selection fixes.

The active gameplay frame still contains the green HUD over a black world.
This is no longer attributable to missing signed or unsigned image views:
the complete append-only trace contains 34,160 sampled successful texture
bindings and zero `NULL VIEW` events. The remaining graphics frontier is the
render-target / resolve / world-draw path after the cinematic-to-gameplay
transition.

The run also proves that the native input path delivers the scheduled flight
commands to the guest:

```text
pitch W:       packet 103, ly=32767
roll D:        packet 105, lx=32767
yaw Q:         packet 107, buttons=0x0100
throttle LMB: packet 109, rt=255
```

Each command is followed by a neutral state. HUD deltas are visible for yaw
and throttle, but full aircraft/world-state effects cannot be visually
qualified until the world render is restored.

## Evidence

```text
runtime lane: bridge evidence bench
binary SHA-256:
8025867a0d85407f6148128f7a186fd7150f1eef44bbe444efa5124364de0977

trace:
reports/logs/cycle-673-bridge-mission1-signedness/ac6recomp-follow.log

captures:
step-69-mission-launch.png             textured hangar / aircraft
step-72-mission-flight-candidate.png   objective map
step-75-post-objective-confirm.png     rendered terrain cinematic
step-78-flight-hud-baseline.png        active HUD, black world
step-81-flight-pitch.png
step-84-flight-roll.png
step-87-flight-yaw.png
step-90-flight-throttle.png
mission1-render-grid.png
```

The harness was intentionally stopped after the final capture. Exit 130 is
not a guest crash.

## Low-oracle next test

Do not repeat the complete fresh route until a single backend hypothesis has
been selected statically. Compare the indirect EDRAM dump/copy resolve path
currently forced by AC6 with the direct host resolve path, first on a reused
local profile and a bounded capture. Enable verbose per-draw logging only for
the cinematic-to-HUD window. Accept a change only if it restores world pixels
or produces a named render-target/resolve divergence; a merely different
black frame is not progress.

The RexGlue fixes remain executable evidence. Their invariants must be moved
to the AC6-owned Vulkan renderer rather than making RexGlue a product
dependency.
