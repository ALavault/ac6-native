# Cycle 674 — direct-resolve A/B and LOD frontier

Date: 2026-08-03

## Result

A single deterministic Mission 1 replay changed only the resolve policy:

```text
ac6_force_safe_direct_host_resolve=false
direct_host_resolve=true
```

The result is visually equivalent to cycle 673 at the qualified checkpoints:
the F-16C and hangar are textured, the overhead cinematic renders terrain,
and active gameplay renders HUD elements over a black world. Direct host
resolve neither restores the gameplay world nor breaks the earlier material
fix. The hypothesis that AC6's forced indirect EDRAM dump/copy path is the
primary cause of the black world is rejected.

The run was stopped immediately after the final scheduled control capture;
exit 130 is intentional.

## Evidence

```text
reports/logs/cycle-674-direct-host-resolve/render-grid.png
reports/logs/cycle-674-direct-host-resolve/step-69-mission-launch.png
reports/logs/cycle-674-direct-host-resolve/step-75-post-objective-confirm.png
reports/logs/cycle-674-direct-host-resolve/step-78-flight-hud-baseline.png
reports/logs/cycle-674-direct-host-resolve/step-87-flight-yaw.png
reports/logs/cycle-674-direct-host-resolve/ac6recomp-follow.log
```

The bridge binary is unchanged from cycle 673:

```text
SHA-256 8025867a0d85407f6148128f7a186fd7150f1eef44bbe444efa5124364de0977
```

## Refined graphics boundary

The hangar establishes that aircraft mesh loading, geometry and material
decoding work in one material family. The overhead cinematic establishes that
the environment/terrain is textured, but its aircraft meshes remain visibly
white. Before the signed-view fix, the hangar and cinematic had broader
white-material symptoms; that fix closed the hangar/terrain cases, not the
cutscene-aircraft case. The active-HUD black frame is therefore a separate
world-render problem and not evidence that the aircraft asset is absent.

The next comparison must be draw-level and local to the transition:

1. catalogue vertex/pixel shader identities, vertex-fetch formats, render
   targets and depth state for a textured hangar aircraft draw;
2. catalogue the equivalent cinematic aircraft/terrain draws;
3. compare them with gameplay world and flight-LOD draw candidates;
4. classify whether gameplay draws are absent, rejected before submission,
   depth-clipped, written to an uncomposed target, or use a distinct broken
   shader/fetch variant.

No additional full replay is justified until instrumentation emits that
compact per-pass comparison. Asset extraction should then be restricted to
the identified aircraft/environment records and must retain table index,
archive range, representation sizes, hashes and detected container metadata.
