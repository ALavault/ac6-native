# MoveEffect metadata-video candidate

Date: 2026-07-15

## Selected native candidate

`Scene` group index 5 is the first verified non-zero candidate:

| Native Scene group | Export CUT | Scene FHM path | Camera samples | Frame 1 MoveEffect counts |
| ---: | --- | --- | ---: | --- |
| 5 | `0005` | `22.1.5` | 220 | serialized 3, resolved E_EFFMOVE 3, validated MOP metadata 3 |

The candidate is closed by two independent local routes:

- `relations.csv` maps CUT `0005` to Scene FHM path `22.1.5`;
- the native shell smoke/capture invocation for `--scene-group 5` returns
  `camera_states=220`, a bounded Tcam state at serialized frame 1, and the
  three equal MoveEffect counters above.

The group remains an inspector selection in stable archive order. It does not
prove campaign activation, spawning, effect appearance, or a transition to
another Scene group.

## Capture plan

After the next significant shell build is staged in root `bin/`, record this
native metadata-only presentation under Xvfb for 30--120 seconds. Use the
existing Scene controls (Start / Xbox Start starts the native serialized
camera-sample playback) and label the timeline as `MoveEffect metadata:
serialized/resolved/validated`.

The capture must show the count ticks and the regular native partial Scene
shell only. It must not claim the MOP payloads produce particles, lighting,
post-processing, duration, or any other effect rendering. Encode the captured
frames with GStreamer VP8 to WebM only after the staged `bin/ac6-scene-shell`
is verified under the intended Xvfb command.

## Rejected alternatives

CUTs `0002`, `0003`, `0004`, `0006`, `0007`, `0009`, `0011`, `0012`, `0015`,
`0032`--`0037` contain MoveEffect events according to their ordered export,
but an event-bearing CUT alone is insufficient: a video requires the separate
bounded Tcam replay route. No camera is synthesized for a CUT that lacks that
route.

## Validation command

The following command completed against the locally built native shell with
SDL's dummy video backend and captured a BMP inspector frame:

```
SDL_VIDEODRIVER=dummy ac6-scene-shell --campaign-selector 1 \
  DATA.TBL DATA00.PAC DATA01.PAC --scene-group 5 \
  --capture-frame 1 GROUP5_FRAME1.bmp
```

Its JSON result reported `camera_states=220`, `first_frame=1`, and
`serialized_move_effect_commands=3`,
`resolved_e_effmove_resources=3`,
`validated_e_effmove_mop_metadata=3`.
