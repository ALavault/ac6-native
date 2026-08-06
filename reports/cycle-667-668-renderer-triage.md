# Cycles 667–668 — renderer triage without oracle

Date: 2026-08-03

## Result

The dominant null-texture fault was isolated and corrected without an oracle
run. AC6 shaders may request a signed fetch while the binding's swizzled signs
are entirely unsigned. The normal Vulkan texture path selected
`image_view_signed` solely from the shader bit, although that view had never
been created. The existing 3D-as-2D path already used the required combined
predicate.

The normal path now selects the signed view only when both conditions hold:

```text
shader requests signed fetch
AND binding has at least one signed swizzled component
```

This RexGlue change is a temporary executable specification, not a final
product component. The durable contract is the sign/view-selection invariant;
it must move into the AC6-owned Vulkan texture system when that subsystem
replaces the evidence runtime.

In the pre-fix gameplay window from cycle 662, all 1,467 sampled `NULL VIEW`
events were `no-image-view`; the dominant signature was a signed shader fetch
against an unsigned-only binding. A short existing-profile smoke test after
the correction produced zero sampled null-view events, 28 valid sampled binds
and 1,802 presents. The formerly grey/black output became a textured cloudy
sky.

This proves the view-selection correction, but not yet the Mission 1 world or
aircraft-material correction: cycle 667 did not start from the qualified fresh
Mission 1 route.

## Cycle 668 boundary

A fresh, bounded native run reached the save UI state `type=6`, `selector=4`
and remained responsive for 4,154 presents. The capture is a blue gradient
without visible UI. No blind input was added.

This is evidence that the `type=6` route problem is distinct from the fixed
null-view defect. It also explains why repeated timing changes around this
state did not improve the route.

## Validation

```text
ac6recomp full target build: PASS
binary SHA-256:
060db093b2a29c3bbcbea45b456cd0f99b1250c2633c41b2c1d892b3dc5070fd

ctest --test-dir build-rt -R ac6:
9/9 PASS, including ac6_real_asset_contract

cycle 667:
bounded native smoke, zero sampled NULL VIEW events

cycle 668:
fresh native route, type=6/selector=4 stable, 4,154 presents
```

Capture:

```text
reports/logs/cycle-668-type6-render/step-20-fresh-save-type6.png
```

## Low-oracle protocol

1. Treat GPU telemetry as the primary detector: null views, format/sign
   mismatch, render-target writes, texture binds and presents.
2. Use short native checkpoints that exercise one transition and preserve a
   screenshot plus the exact telemetry window.
3. Re-enter Mission 1 only after the route predicate is deterministic; capture
   runway, aircraft and first HUD frame in the same run.
4. Compare captures numerically and require disappearance of the relevant GPU
   anomaly before visual acceptance.
5. Request one oracle capture only if native evidence leaves a named visual
   ambiguity. The request must specify the exact state, frame, camera/input and
   expected comparison artefact.

## Next boundary

Determine why save UI `type=6`, `selector=4` has no visible controls from guest
state and render telemetry, without another long run. Then perform one fresh
native Mission 1 run with three scheduled captures: runway aircraft, end of
cinematic, and first active-HUD frame. That run decides separately whether the
signed-view correction fixes the white aircraft and the black gameplay world.
