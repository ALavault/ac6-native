# AC6 Linux Scene shell and first native camera state

Date: 2026-07-15

## Closed data identity

The first CUT event stream makes the two `MoveCamera` words non-neutral. Across
the first 120 frames the payload pattern is:

- word 0 high half: Scene object id (`1` for `MoveCamera`);
- word 0 low half: zero flags;
- word 1 high half: Tcam key/frame (`1` through `120`);
- word 1 low half: zero flags.

The other per-frame events use consecutive Scene object ids `2` through `18`.
Together with the already proven path-record-N/resource-member-N join, object
id 1 is therefore the one-based identity of Scene path/resource index 0:
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`.

Its three GYZ records are now consumed as bounded native camera tracks:

- record 0: 121 four-float position keys and matching big-endian `u16` times;
- record 1: 121 four-float angular-orientation keys and matching times;
- record 2: one float vertical field of view.

At CUT frame 1 the native state is position
`[-15824.801758, 3284.871826, -1023.996643, 1]`, orientation approximately
`[-1.284656, 0.284061, 0.000019, 0]`, and vertical FOV approximately
`0.761 radians`. A native 4x4 transform is produced with an explicitly named
XYZ modernization convention. The exact retail Euler multiplication order is
still open and is not claimed by that matrix.

## XEX receiver boundary

`0x8236b920` does not load a serialized Tcam pointer directly. Its `this+4`
field is a runtime dispatch context; the context's first pointer leads to the
event target at offset `+0x10`, whose virtual slot `+0x20` receives the exact
tag, payload and size returned by `0x8236dc70`. The dynamically supplied target
class/vtable address is not recoverable from this static call site alone.

The concrete data receiver identity is nevertheless closed at the serialization
boundary: `MoveCamera(0x00010000, 0x000N0000)` selects Scene object 1 and Tcam
key N. This is the join used by the native replay. It avoids assigning a
speculative class name to the unresolved runtime vtable.

Additional evidence is retained in
`reports/nfic-camera-callback-receiver.log` and
`reports/nfic-driver-followup.log`.

## Scene-to-MDLP world join

The first CUT also makes two animated rigid objects directly actionable:

- Scene object 3, `Tlod__r_f16c_t1__01.mop`, joins by the proven `r_f16c`
  identity to MDLP element 76 and its retail NDXR member;
- Scene object 4, `Tlod__r_f18f_t1__01.mop`, joins by `r_f18f` to MDLP
  element 78 and its retail NDXR member.

The native NDXR parser now consumes the big-endian `0x0200` version,
polyset/object descriptors, polygon descriptors, index clump, vertex clump,
weighted-vertex-additional clump and name clump. It extracts bounded retail
positions and 16-bit topology. The two joined aircraft contribute 9,711
retail vertices and 14,492 retail indices. Their first two MOP records are
sampled independently as position and sparse orientation tracks, so geometry
is placed from the same CUT frame as Tcam rather than displayed as an isolated
model.

This is a deliberately partial world renderer: it covers the first two
animated aircraft and wireframe topology. Materials, Xenos textures, bones,
the remaining 14 first-CUT objects and exact retail Euler/projection conventions
remain open.

## Linux shell

`ac6-scene-shell` is an SDL3 shell that accepts decoded DATA.TBL entry 9,
locates its first bounded CUT/resource-FHM/Scene triplet, replays all 120
`MoveCamera` camera states and displays the X/Z camera path, current position,
orientation indicator and timeline. It derives the first frame's
dictionary-backed `Rigid`/`AnimRigid` events (`0x2003`/`0x2004`) and joins all
16 serialized objects (Scene ids 3..18) to their same-index MOP transform
tracks and asset-key-matched MDLP models. This provides 39,393 native vertices
and 57,271 indices at CUT frame 120. `C` toggles the current
Tcam projection path.

The world membership is now checked across the full serialized CUT. The
native NFIC collector replays the cut/frame lifecycle and retains only
dictionary-backed zero-flag `Rigid`/`AnimRigid` commands associated with their
active `FrameStart`. Before rendering, the shell confirms that each of the 120
camera frames contains the same ordered 16 Scene-object ids as frame 1. This
prevents a first-frame resource join from being silently displayed after its
serialized tracks disappear; it remains presentation-local, not a gameplay
entity-lifetime claim.

The membership payload now preserves the command kind rather than reducing the
joined records to ids. The first selected frame has 14 `Rigid` and 2
`AnimRigid` commands, emitted as `active_rigid_tracks` and
`active_animated_rigid_tracks` in smoke JSON. The native wireframe palette
distinguishes those two exact command forms only; it does not infer scene
roles, player ownership, or animation semantics beyond the dictionary name.

For the command-line campaign route, the shell now has an explicit native
session state rather than treating decoded entry 9 as an implicit mission
start. It accepts only selector 1 after the exact DPL/resource and physical
record-9 checks, enters `scene_ready`, then transitions to `scene_playing` on
the native Scene-player launch action and `scene_complete` only when the CUT
player reaches its terminal sample. The smoke JSON exposes this session phase.
This is a Linux UI boundary around the proven archive route; it does not
reconstruct the retail campaign-menu receiver or assert mission gameplay.

Controls are Left/Right (single-frame step), PageUp/PageDown (ten-frame step),
Home (first frame), Space (play/pause), `C` (Tcam/inspection camera), `A`/`D`
(inspection orbit), `+`/`-` (inspection zoom) and Escape (quit). `--smoke`
creates a hidden window, renders once and emits a machine-readable diagnostic.
`--capture-frame CUT_FRAME PATH.bmp` selects a dictionary-proven MoveCamera
frame before rendering, so the camera and joined MOP transform update are also
deterministically capturable. Frames 1 and 120 visibly differ; see
`FUNCTION_8236BF20_DYNAMIC_TRACK_CONSUMER_REPORT.md`.

J/L are native keyboard surrogates for raw XInput D-pad-left/D-pad-right. They
flow through `0x821CE088`, the recovered default logical slots 8/9, and the
same bounded timeline-selection helper used by the keyboard controls. The
selection is clamped, never wraps, and does not alter the CUT lifecycle. Since
the retail manual documents D-pad input as context dependent, this only makes a
proved physical-to-logical input path interactive in the Linux Scene-shell
inspection context; it does not claim retail in-mission D-pad parity.

Return now drives a separate, visible player-input diagnostic. It feeds raw
XInput-A through the exact `0x821CE088` canonical mapping, the retail default
table recovered at `0x821BE268`, `0x82215140`, and the `0x82214F88`
just-pressed edge. The top-right marker and title expose the state. This proves
native input reaches a recovered logical-command observable; it does not yet
name or execute a flight action. See `FUNCTION_821CE088_PLAYER_INPUT_REPORT.md`.
`--capture-input PATH.bmp` deterministically presents the pressed diagnostic;
the checked PNG is `captures/player-input-a-pressed.png`.
Raw A activates default logical slots 0 and 23. Their aircraft meaning remains
open, so the marker stays isolated from Scene-object transforms.

The initial camera boundary is now fail-closed in the library rather than an
implicit `frames.front()` convention. `select_initial_scene_camera` requires
the first three dictionary-backed events to be `CutStart`, `FrameStart(1)`,
and zero-flag `MoveCamera(object 1, frame 1)`, resolves object 1 through the
adjacent resource FHM, and is checked against the full replay. This proves the
initial camera inside the selected CUT. It does not prove that runtime mission
state activates the first physical Scene group; the smoke JSON exposes that
separate flag as false. See `MISSION_VISUAL_BOOTSTRAP_REPORT.md`.

The complete retail smoke result after the full first-CUT object join is:

```json
{"status":"ok","camera_states":120,"first_frame":1,"scene_object_id":1,"position":[-15824.8,3284.87,-1024],"world_objects":16,"world_scene_object_ids":[3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18],"retail_vertices":39393,"retail_indices":57271,"world_renderer":"native-partial"}
```

## Verification

- GCC native build: 21/21 CTest tests pass.
- Clang with AddressSanitizer and UndefinedBehaviorSanitizer: 21/21 pass.
- SDL3 dummy-driver smoke passes on the real first CUT and MOP.
- Full decoded-entry-9 smoke passes from the retail DATA.TBL/PAC extraction.

The unresolved dynamic NFIC receiver is not required for this observable
result: the exact serialized object/key join, MOP tracks and retail MDLP/NDXR
identity are all independently bounded. Recovering that class remains useful
for behavioral parity but is no longer on the critical path to first geometry.

No MinGW/Zlib work was performed in this slice.

## Material/texture identity follow-up

The model/texture MDLP pairs `76/77` and `78/79` close 115 exact NDXR
texture-id to NTXR-GIDX references. The same identifiers have 227 exact
big-endian occurrences in the bundles' four MATE payloads. The first matched
NTXR is now a proven six-entry wrapper with a 507,904-byte aggregate data tail.
Its first matched entry decodes exactly as a 512x512 BC3 atlas using Xenos
tiled-2D addressing and `8-in-16` endian conversion. Exact MATE batch ordinal,
material first texture, NDXR first texture, NTXR GIDX and UV0 agreement now
guards presentation of 52 first-aircraft polygons. All NDXR indices are local
(`index_oob=0`). The second aircraft and two UV-incomplete first-aircraft
batches remain wireframe; the renderer therefore stays `native-partial`.

`--capture PATH.bmp` now saves the rendered smoke frame. The visible checked
capture is `captures/first-native-textured-aircraft-frame.png`, with its direct
SDL BMP source beside it. See `AC6_MATERIAL_TEXTURE_LINK_REPORT.md`.

## Native Scene playback lifecycle

The shell now owns a native CUT lifecycle rather than treating the 120 decoded
`MoveCamera` samples as an indefinitely wrapping inspection index.
`ScenePlaybackState` admits only `ready -> running -> paused -> running` and
`running -> complete`; a zero-sample Scene, an advance while paused, a restart
after completion, and an implicit wrap to frame 1 are rejected. The final
camera sample remains selected when the lifecycle reaches `complete`.

The campaign executable exposes this path through:

```text
--campaign-selector 1 DATA.TBL DATA00.PAC DATA01.PAC --run-scene-steps 119
```

The direct Linux run resolves the selector-1 route to DATA.TBL entry 9, renders
the final Scene sample, and emits `"native_scene_playback_phase":"complete"`
plus sample `119`. Its CTest gate matches that output using the real PAC assets.
This is native C++/SDL runtime code, not a console executable or emulator path.

In the interactive executable, Space operates the native player directly. P
uses raw XInput Start (`0x0010`) through the recovered `0x821CE088` transform
and default `0x821BE268` action table; the resulting logical slot 4 operates
the same native pause gate. The manual supports Start as the physical pause
control, but the opaque retail pause-menu receiver has not been recovered, so
this is a native Scene-player pause gate rather than a claim of menu fidelity.
Flight, weapons, mission spawning, scoring and actor AI remain outside this
slice.
