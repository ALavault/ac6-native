# Native MoveEffect metadata timeline

Date: 2026-07-15

## Result

`ac6-scene-shell` now presents a visual-neutral MoveEffect timeline for every
replayable Scene group it loads.  A per-camera-frame entry contains only three
bounded counts:

1. serialized `MoveEffect` commands;
2. commands whose one-based Scene/FHM target resolves to an `E_EFFMOVE_` MOP;
3. resolved targets whose MOP validates as the exact two-record metadata form.

The bottom timeline draws a tick for a frame with one or more serialized
commands. Green means all three counts agree; amber means the available
resource/metadata evidence is incomplete. Tick height is `log2(command count)`
only, capped at 18 pixels. It encodes no effect transform, visibility,
duration, particle state, blend mode, or rendering operation.

The window title and smoke JSON expose the same counts as:

```
serialized_move_effect_commands
resolved_e_effmove_resources
validated_e_effmove_mop_metadata
```

The default first Scene group has zero commands at its selected first camera
sample; this is a valid zero result, not an absent parser. Some event-bearing
exported CUTs can lack a replayable Tcam state, but the separate candidate
audit closes a valid non-zero native group (`22.1.5` / CUT `0005`).

## Evidence chain

The display requires all three previously closed boundaries:

- `collect_nfic_frame_effect_commands` accepts only the exact dictionary-backed
  8-byte MoveEffect records under their owning serialized frame;
- `resolve_move_effect_scene_resource` requires the one-based effect-id to
  Scene/FHM association and an `E_EFFMOVE_` path;
- `extract_mop_effect_resource_metadata` accepts only the bounded two-record
  E_EFFMOVE GYZ form (`field_08` values 3 then `0x00010002`).

No rendering API consumes either MOP record payload. The visual path is
strictly an inspector overlay built from these counts.

## Validation

An isolated CMake/Ninja build with `-j 16` built `ac6-scene-shell`, followed
by successful `ac6-nfic-cut-tests`, `ac6-mop-tests`, `ac6-scene-tests`,
`ac6-scene-shell-smoke`, and `ac6-campaign-scene-shell-smoke` tests. The
campaign smoke JSON reports the new metadata fields as available and zero for
its first serialized camera frame.

## Video readiness boundary

The candidate audit identifies a replayable non-zero group suitable for the
next capture. Run the Linux `bin/ac6-scene-shell` under Xvfb, capture 30--120
seconds of the native timeline, and encode the frames to VP8 WebM with
GStreamer. It must label the overlay as serialized-resource metadata and not
as an effect renderer.
