# AC6 mission visual bootstrap boundary

Date: 2026-07-15

## Retail-proved archive and camera edges

The campaign archive edge is exact: bounded campaign selector 1 passes through
`0x821b6e58`, resource id 9, `DPL::[0x9,0]`, hash `0xfc76b3c6`, the recursive
registry lookup and `0x821d1128` to physical `DATA.TBL` entry 9. No inferred
filename is needed for that selection.

Within an already selected Scene group, the initial camera edge is also exact.
The retail `NFIC CUT` parser chain `0x8236eda0 -> 0x8236e2e0 -> 0x8236da78`
exposes events through `0x8236dc70`; `0x8236b920` dispatches them. The supplied
first group begins with the dictionary-proved sequence:

1. `CutStart`;
2. `FrameStart(1)`;
3. `MoveCamera(0x00010000, 0x00010000)`.

The command selects one-based Scene object 1 and track frame 1. The exact
Scene-record-N/resource-member-N join resolves it to entry-9 resource
`22.1.0.1.0`, path
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`. Its retail GYZ data samples to
position `[-15824.801758, 3284.871826, -1023.996643]`, the previously reported
orientation, and vertical FOV at CUT frame 1.

Native `select_initial_scene_camera` now accepts this state only when those
three events occur first, both dictionary names agree, both frames equal 1,
both command flag halves are zero, and the adjacent FHM index join is bounded.
Every mismatch returns no selection. The Scene shell checks this result against
its full CUT replay before rendering and reports the claim flags explicitly.

## Open mission-scene activation edge

The executable path proves loading/registration from entry 9 and the event
selection inside a chosen Scene group. It does not yet prove that campaign
runtime state activates physical group `22.1.0` merely because it is first in
archive traversal order. Accordingly:

- `initial_camera_selection_proved = true` is conditional on the selected
  Scene group;
- `mission_scene_group_activation_proved = false`;
- `spawn_proved = false`.

The current shell may use the first structurally valid group as an explicit
modernization bootstrap, but that ordering is not promoted to retail mission
activation semantics.

## Static-environment boundary

The first group contains only retail resources: the camera, player track,
aircraft/vehicle/radar transform paths and their adjacent GYZ payloads. The
native shell already renders two archive-joined MDLP/NDXR objects, but those
are aircraft-side assets. No executable ownership join yet identifies terrain
or another static-environment mesh for this selected group. This pass therefore
adds no synthetic sky, ground plane, proxy geometry, or guessed environment.
Static-environment rendering remains fail-closed until an archive resource and
its retail Scene/CUT ownership edge are both proved.

## Validation

- real decoded entry-9 SDL dummy-driver smoke: 120 camera states, first CUT
  frame 1, object 1, unchanged retail frame-1 position;
- GCC complete suite: 16/16;
- Clang ASan+UBSan complete suite: 16/16;
- malformed initial frame and nonzero camera flags are rejected by tests.

