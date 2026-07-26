# Ace Combat 6 native reconstruction status

Updated: 2026-07-17 (Europe/Paris)

## Product boundary

The deliverable is a native Windows/Linux reconstruction under
`reconstruction/ace-combat-6/`. This workspace retains the Xbox 360 PowerPC
evidence, Ghidra/re-agent state and proprietary files. The native product does
not embed an emulator or retail assets.

## Implemented native foundation

- bounds-checked big-endian `DATA.TBL` parsing;
- typed selector, PAC-bank and storage-class accessors;
- validation against the actual `DATA00.PAC` and `DATA01.PAC` sizes;
- monotonic range, expanded-size and stored-class invariants;
- recovered `ACE6::CAce6Uncompress` modes: custom LZ, raw DEFLATE and stored;
- exact pi/Machin-derived XOR key generation with the retail 256-entry cycle;
- bounded single-entry extractor and exhaustive archive verifier;
- recursive FHM asset-manifest generator with a depth-16 guard, neutral
  metadata fields and exact signature-only resource classes;
- bounded `NDXR` model parser with exact object/polygon descriptors, named
  index/vertex/additional/name clumps, retail positions and 16-bit topology;
- bounded `NSXR` wrapper parser with five safe neutral region slices;
- bounded `MATE` wrapper parser with three safe neutral region slices;
- portable packed-handle resolver for the XEX `0x40`-stride runtime record
  pool, with native bounds checking;
- exact campaign selector-to-DPL-resource mapping, DPL key formatting and
  registry CRC generation for the `0x820a85e0` load path;
- bounded child-1 `MDLP` directory parsing matching `0x8228e988`,
  `0x8228e9a8`, and `0x8228e9b8`;
- bounded fixed-record `Scene/` path-table parsing and absolute FHM ancestry
  mapping for entry-9 children 22 and 23;
- exact index-based resolution of all 553 entry-9 Scene paths through adjacent
  resource FHMs, with bounded `GYZ` wrapper parsing and `NFIC CUT` group proof;
- fixed-stride `GYZ` record-table parsing and nine-chunk `NFIC CUT` parsing,
  including its bounded event stream and identifier/string dictionary;
- native replay of the dictionary-proven cut/frame/camera-event state and a
  deterministic raw-plus-structured Scene MOP/CUT remaster export;
- bounded Tcam position/orientation/FOV sampling, sparse generic MOP transform
  sampling, and an SDL3 Linux Scene shell rendering two joined retail aircraft;
- portable C++20 implementation, synthetic tests and sanitizer gate.
- address-bounded native input normalization, configurable logical-button
  remapping and pressed/released edges from `0x821CE088`, `0x82215140` and
  `0x82214F88`, with a visible non-flight SDL diagnostic.
- frame-lifecycle-gated `NFIC CUT` camera replay: `MoveCamera` is rendered
  only while a dictionary-proven `FrameStart` remains active and its payload
  frame matches the active frame; a matching four-byte `FrameTerminate` and
  then `CutTerminate` close that state before another camera event can be
  accepted.
- SDL3 native Scene-player operation for Bluetooth Xbox controllers: SDL
  standard A/D-pad/Start events feed the recovered raw masks and then the
  existing `0x821CE088` normalization and logical-button table. Back toggles
  the native camera inspection presentation, Guide exits, and F11 toggles
  fullscreen. This is an evidence-backed input route into the bounded CUT
  player, not a claim of retail flight or pause-menu behaviour.
- The entry-9 native Scene inspector exposes serialized Scene-group selection
  interactively (`--scene-group INDEX`), keeping the selected index and FHM
  archive path in its title and JSON. This is a deterministic inspector route,
  not evidence that campaign progression activates the selected group. The
  asset-backed gate covers six independently replayable joined CUT groups:
  `22.1.0` (120 camera states), `22.1.1` (80), `22.1.5` (220), `22.1.10`
  (430), `23.1.2` (210), and `23.1.4` (420). The child-23 groups prove that the
  native chain also handles the independently serialized child-23 subtree; this
  does not claim a retail mission-order relationship between any of them.
- The inspector now has a fail-closed replay catalog generated from the same
  entry-9 loader: only the six groups above have both a bounded camera replay
  and joined world. `[`/`]` and the Bluetooth Xbox left/right shoulder buttons
  move through that catalog, reset the native playback state, and retain the
  exact serialized index/FHM path in the title and JSON. This is native UI for
  comparison and inspection only, not a retail campaign transition.
- exact `CModeTaskTitleMovie` update-state representation from `0x821B9048`:
  auxiliary-ready/skip/complete predicates, three-tick mode handoff and the
  generic `{1,3}` flow tuple are exposed as side-effect-free native effects.
  It is intentionally isolated from campaign launch and mission gameplay.
- The pinned Windows Xenia Canary oracle now has a Wine/Vulkan launcher with
  keyboard mode and an AZERTY-compatible `Start=Return` preflight. A user
  session confirms the startup order developer splash -> copyright ->
  skippable cinematic -> title. This is a startup-horizon observation only;
  it does not qualify title-to-campaign selection, mission loading or flight.
  See `reports/cycle-97-xenia-wine-startup-sequence.md`.

## Executed retail gate

The native inspector validated the supplied European revision:

- 926 records and two PAC banks;
- 466 records in bank 0 and 460 in bank 1;
- 800 compressed-class records and 126 stored-class records;
- 2,914,429,232 stored bytes represented by table entries;
- 5,424,368,676 expanded bytes declared;
- every stored range is monotonic and within its selected PAC.
- all 926 payloads decode to their exact declared expanded size;
- all 926 decoded payloads begin with the `FHM ` inner-container signature.
- all 926 top-level FHM directories pass bounded parsing, with 4,820 member
  slots and 2,811 non-empty members.

This proves the table layout, range semantics, encryption key stream and payload
codec against the complete supplied retail archive. It does not yet parse the
resource-specific inner payloads or make the game playable. The recursive FHM
gate emits 56,514 rows and finds 5,435 nested containers, 8,006 `NTXR`, 2,228
`NDXR`, 1,549 `NFIC`, 1,293 `Scene`, 1,029 `NFH`, 733 `MATE`, 546 RIFF and
11,204 empty slots. The remaining 23,861 non-empty payloads retain the neutral
`binary` class and their four-byte prefix only.

The first resource-specific slice now validates all 8,006 `NTXR` records:
7,993 complete wrappers have bounded descriptor, `eXt`, `GIDX` and texture-data
boundaries, while 13 are explicit 16-byte header references. Xenos descriptor
bit fields remain neutral until their dimensions, format and tiling semantics
are independently proven.

The second resource-specific slice validates all 2,228 `NDXR` records. Every
wrapper has version byte 2, an exact declared size and three nonzero,
16-byte-aligned region lengths whose sum remains inside the payload. Their
graphics semantics remain deliberately unnamed; see `NDXR_STRUCTURE_REPORT.md`.

The XEX-backed `0x822c2148` slice now decodes its exact `0x30`-stride NDXR
record loads and flag-bit gate within a fail-closed native capacity. The retail
gate covers 250,766 bounded slots; 248,325 pass the original bit test. The four
float meanings remain unnamed because no caller has yet been recovered; see
`NDXR_FUNCTION_822C2148_REPORT.md`.

The 22-instruction leaf `0x822c31e8` now has a bounded portable search for its
signed key at record offset `0x24`. Eleven XEX call references establish the
runtime search contract. Corpus correlation finds only keys 2 and 4 among ten
profiled fixed caller keys, so direct identity between serialized slots and the
complete runtime container remains unproved; see
`NDXR_FUNCTION_822C31E8_REPORT.md`.

The four-instruction leaf `0x822c3150` now has a portable resolver for its exact
26-bit packed index and `0x40`-byte runtime-record stride. The native span model
replaces the XEX base pointer loaded at container offset `+0x20` and rejects
incomplete or out-of-capacity records. Its three-instruction tail wrapper at
`0x822c8e48` supplies a handle from `+0x04` and a container from `+0x2c`.
Record semantics remain neutral; see `FUNCTION_822C3150_RECORD_POOL_REPORT.md`.

All 51 `NSXR` wrappers now pass exact-size and five-region boundary validation.
The first four offsets are constant at `0x60/0x70/0x80/0x90` in this corpus;
the fifth has 26 observed values. Region meanings remain unnamed; see
`NSXR_STRUCTURE_REPORT.md`.

All 733 `MATE` wrappers now pass three-region boundary validation. The first
offset is `0x30` throughout this corpus, but the next two have 18 and 66 values;
their material/shader meanings remain unassigned. See `MATE_STRUCTURE_REPORT.md`.

## Next native front

The DPL-registry-to-physical-archive assignment is now closed for the campaign
low-id path. The XEX path proves that bounded campaign selector 1 maps through `0x821b6e58` to
resource id 9, key `DPL::[0x9,0]` and hash `0xfc76b3c6`, followed by recursive
registry lookup, then through `0x821d1128` to physical `DATA.TBL` record 9.
Archive decoding is no longer the blocker: all 926 entries decode,
and 1,293 `Scene` payloads are structurally inventoried.

The DPL and physical index namespaces are now proven non-identical in general:
valid DPL ids exceed `0x75e`, while the direct archive branch is restricted to
ids below `0x39d`; larger ids take a special-resource route. The native
`ac6-mission-diagnostic` reports selector 1's physical entry as 9. See
`DPL_ARCHIVE_HANDLE_CHAIN.md`.

The decoded entry-9 diagnostic now deterministically reproduces 1,111 recursive
manifest rows, including 44 `Scene` payloads. Its child 1 is a 29,097,984-byte
`MDLP` directory with 94 elements. Exact XEX helpers expose those elements to
`0x820a7070`, which makes two complete registration passes and later selects
individual elements while constructing runtime objects. See
`ENTRY9_CHILD1_CONSUMER_REPORT.md`.

The construction-loop owner is now typed by Microsoft RTTI as
`X360UnitManager`, derived from `ACE6::CAce6UnitManager`. Its base constructor
clears an exact 256-slot pointer/word table. The entry-9 virtual factory returns
a runtime object whose `+0x50/+0x54/+0x58` floats are cleared before a selected
MDLP resource is attached at `+0x15c`. This establishes a gameplay unit-owner
and transform-bearing object boundary, but not yet an aircraft subtype, spawn
position, or semantic function name. See `ENTRY9_X360_UNIT_MANAGER_REPORT.md`.

Factory selector 1 is now proved by final-vtable RTTI as
`ACE6::CAce6UnitPlayer`; selector 2 is `ACE6::CAce6UnitOtherPlayer`, both with
`ACE6::CAce6Unit` in the hierarchy. The native address-based factory-evidence
API assigns RTTI only to these two cases; selectors 3 through 6 retain exact
vtable/callback addresses with unknown RTTI. It also exposes the exact zero
vector at `+0x50` while keeping aircraft, spawn and position flags false. GCC
and Clang ASan+UBSan each pass 16/16 tests.

Selectors 3 through 6 now have exact vtable/callback evidence but no RTTI;
their callback ignores the supplied record argument. Owner slot `+0x14` is
bounded for direct selectors 2/3 and its complete 15-key selector-4 table.
Those callbacks bind record/nested pointers without a proved transform write.
The native API mirrors these rows while keeping RTTI, aircraft, spawn and
position false; see `ENTRY9_X360_UNIT_MANAGER_REPORT.md`.

The reusable factory-object method dossier now keeps slot `+0x50` (the
post-factory record callback) separate from slot `+0x54` (the selected-pointer
predicate). Selectors 1/2 share `0x822ddbe8` for both slots; selectors 3–6 use
`0x82297a40` and `0x822974c8` respectively. This is an exact method-family
reuse boundary, not a complete C++ class or a gameplay semantic claim. It is
implemented by `function_820a7f48_virtual_method_evidence` and covered by the
unit-factory tests.

The post-factory chain is bounded through retail insertion `0x8226f050` and
frame traversal `0x8226ecb0`, called from `0x8226a310` at `0x8226a508`.
Native evidence records the exact 1024-entry collection layout, queried
virtual slots, flag masks and direct-call arguments. No executable edge to
`MissionAircraft`, spawn semantics, or a proved position field was found, so
all corresponding claim fields remain false.

Separately, retail `0x822f31e8` proves that the selected global object's
`+0x50/+0x54/+0x58` floats are formatted as `X/Y/Z`. The native evidence marks
that local XYZ semantic true, while keeping the identity of that selected
object as an entry-9 factory result, MissionAircraft, and spawn position false.
The intervening slot-`+0x54` condition is now exact: `0x8226f050` tests only
the low byte returned in `r3`. Selectors 1/2 preserve their dynamically
allocated, merely `0x10`-aligned object pointer; selectors 3-6 do the same when
flag `+0x60 & 0x100` is clear. The specifically proved entry-9 record-callback
armed path for selectors 3-6 returns zero. Runtime arena address residue still
blocks a static entry-9-to-XYZ identity, and debug XYZ formatting remains no
evidence of a spawn operation.
An executable-wide direct-store inventory adds only collection initializer
`0x8226eb88`, which zeros `+0x1008/+0x100c` at `0x8226eb98/0x8226eb9c` while
also clearing the exact 1024-entry and 16-pointer layout. All other literal
offset stores belong to different structures. Therefore `0x8226f050` is the
sole nonzero writer of these selected pointers; no alternate retail path
closes a particular entry-9 result to the debug XYZ object.

All 94 elements are now individually bounded and inventoried. They are FHM
packages containing 292 `NDXR`, 381 `MATE`, 86 `NTXR`, 47 nested FHM and 942
neutral binary members. No `Scene` signature occurs inside child 1. The
per-element retail artifact is
`reports/entry9-child1-mdlp-inventory.csv` with recorded source and artifact
SHA-256 provenance.

The 44 entry-9 `Scene` payloads are now exactly localized outside child 1:
32 occur below top-level child 22 and 12 below child 23. They are flat,
non-empty tables of NUL-terminated `Scene/` paths in fixed `0x80`-byte records,
with 553 records total. The absolute-offset artifact is
`reports/entry9-scene-inventory.csv`; see
`ENTRY9_SCENE_PATH_TABLE_REPORT.md`.

All 553 paths now resolve by record index to the same-index member of an
immediately preceding resource FHM. Every one of the 553 payloads satisfies a
bounded `GYZ` wrapper contract, and each of the 44 path/resource pairs has an
immediately preceding `NFIC CUT` sibling. The complete deterministic mapping is
`reports/entry9-scene-path-resolution.csv`; see
`ENTRY9_SCENE_RESOURCE_RESOLUTION_REPORT.md`.

The resolved `GYZ` payloads now expose 3,105 bounded `0x30`-stride records.
The first `Tcam__c01.mop` has three records, while its adjacent `NFIC CUT`
payload proves the serialized sequence `CutStart`, `FrameStart(1)`, then
`MoveCamera` through chunk `0x3040` and its `0x3041` dictionary. All 44 state
payloads validate, covering 169,908 event records. See
`ENTRY9_TCAM_NFIC_CUT_REPORT.md`.

The exact XEX NFIC event path is now recovered through `0x8236eda0`,
`0x8236e2e0`, `0x8236da78`, `0x8236dc70`, `0x8236db48`, and dispatcher
`0x8236b920`. `MoveCamera` now resolves one-based Scene object 1 to path and
resource index 0 (`Tcam__c01.mop`) and its second high half to the Tcam key.
The first CUT yields 120 native camera states. The runtime target is known to
sit at dispatch-context first-pointer offset `+0x10`, but its dynamically
supplied vtable address remains open; see `NFIC_XEX_EVENT_CONSUMER_REPORT.md`.
Native `select_initial_scene_camera` now fails closed unless the first three
events are dictionary-backed `CutStart`, `FrameStart(1)`, and zero-flag
`MoveCamera(object 1, frame 1)`, followed by the bounded adjacent-FHM index
join. This proves the initial camera inside a selected Scene group. Runtime
activation of physical group `22.1.0` is still open, so archive-first traversal
is not claimed as retail mission-scene selection. Static environment geometry
also remains open and no synthetic substitute is generated; see
`MISSION_VISUAL_BOOTSTRAP_REPORT.md`.

An SDL3 Linux shell now loads decoded entry 9 or reaches that same payload from
the proved campaign-selector-1 -> DPL resource 9 -> physical `DATA.TBL` entry
9 route. It locates the first Scene group, replays those camera states, and
derives each first-frame serialized `Rigid`/`AnimRigid` Scene command from the
NFIC dictionary (`0x2003`/`0x2004`) and joins the resulting one-based ids 3
through 18 to their same-index MOP transform and asset-key-matched MDLP model.
The supplied archive yields 16 animated native objects, 39,393 vertices and
57,271 indices at the final CUT frame. Objects 3/4 remain the two exact
MATE/NDXR/NTXR diffuse-bound `r_f16c`/`r_f18f` joins (52 and 63 polygons); the
other 13 joined objects remain geometry-only until their material identities
are closed. Timeline, inspection orbit/zoom and Tcam-toggle controls are
present. The smoke reports `world_renderer=native-partial`; this remains CUT
presentation, not a player-aircraft, mission-spawn, collision, weapon, or
flight-control claim. See `AC6_LINUX_SCENE_SHELL_REPORT.md`.

The native shell no longer implicitly binds every inspection request to the
first traversed Scene triplet. `collect_scene_groups` now enumerates each
structurally valid adjacent `NFIC CUT`/resource-FHM/`Scene` group in stable FHM
directory order and retains its dotted archive provenance. The campaign route
can explicitly inspect a selected index with `--scene-group INDEX --smoke`.
Group 0 remains `22.1.0` (120 camera samples, 16 joined CUT-local objects),
while the independently executed group-1 gate is `22.1.1` (80 camera samples,
17 joined CUT-local objects). Both remain selected serialized presentation
groups; neither is promoted to a campaign activation, spawn or flight claim.

The first CUT's native world join is now frame-lifecycle verified rather than
being a first-frame-only assumption. `collect_nfic_frame_scene_object_tracks`
replays `CutStart`/`FrameStart`/`FrameTerminate`/`CutTerminate`, accepts only
dictionary-backed zero-flag `Rigid`/`AnimRigid` commands whose frame agrees
with the active lifecycle state, and rejects malformed or unterminated input.
The Linux shell requires every one of its 120 replayed camera frames to carry
the same ordered 16-object membership as frame 1 before presenting a
persistent world. This is a CUT-local serialization fact, not evidence of a
mission object lifetime beyond that CUT.

The native player now exposes a narrow, fail-closed campaign-to-CUT session
boundary. `CampaignSceneSessionState` accepts only the independently verified
selector-1 -> `DPL::[0x9,0]` -> physical `DATA.TBL` record-9 route. Its native
launch action can start a ready CUT once, synchronizes only running/paused or
completed playback, and rejects other selectors, records, restart attempts and
unexpected playback state. The SDL shell reports that native session phase in
campaign mode. This is an operable Linux presentation transition, not a claim
that the unresolved retail campaign menu or mission activation path has been
recreated.

The closest executable-side dynamic track consumer is now bounded at
`0x8236bf20`: it validates a frame interval and mode, walks the object track
array and forwards both floating inputs to `0x8236eab0`, which selects a key and
dispatches the property update. Deterministic frame-1/frame-120 capture proves
the joined aircraft and camera state change under native scrubbing/playback.
This is cinematic control, not yet player flight input; see
`FUNCTION_8236BF20_DYNAMIC_TRACK_CONSUMER_REPORT.md`.

A distinct player-input path is now bounded. `0x821CE088` polls four devices,
normalizes XInput-layout button masks into four `0xA0`-stride canonical states,
and `0x82215140` maps those states through 32 configurable masks.
`0x82214F88` computes exact just-pressed and just-released fields. The SDL shell
routes Return through the recovered raw-A mapping and the retail default table
to logical slots 0 and 23, then to a visible diagnostic marker. No flight or weapon meaning is
assigned yet; see `FUNCTION_821CE088_PLAYER_INPUT_REPORT.md`.

`0x821BE268` now closes the complete 32-mask default table for all four
controller blocks in the third logical-input context. Raw A reaches retail
logical slots 0 and 23; eight analog sources occupy slots 10–17. A corrected
15,333-function export found opaque pressed-state consumers and one generic
slot-10 threshold reader, but no write to proven aircraft state. The shell
therefore retains its separate diagnostic and does not move or fire the
aircraft; see `FUNCTION_821BE268_DEFAULT_BINDINGS_REPORT.md`.

The first consumer classification is now closed without gameplay relabeling.
RTTI proves `0x8214C038` is a virtual method of `CSelectAircraftManager`, while
the separate `CSelectAircraftCamera` vtable is identified independently. The
analog-assisted routine entered at `0x820DB500` can measurably OR a positive
directed analog sample into current, just-pressed, and repeat conditions, but
its damaged action-to-axis switch table and event-only receiver do not prove a
player-aircraft or camera mutation. The bounded condition slice is native and
tested; re-agent passes the smallest complete leaf `0x821B3870` in one round.
See `FUNCTION_820DB500_CONSUMER_EFFECTS_REPORT.md`.

The exact three-state subtransition observed at manager `+0x8D54` is now
native as `function_8214c038_next_selection_state`: `0 -> 1`, `1 -> 2`, and
`2 -> 1`; unobserved values fail closed. GCC and Clang ASan/UBSan test it as
part of their 17-test matrices. This remains aircraft-selection presentation
state only and is deliberately not connected to in-flight controls, weapons,
or camera motion.

The event receiver is now identified by pointer identity rather than matching
offsets. The SWG context initialization at `0x820DBD90..0x820DBF84` allocates
the sole 0x150-byte object constructed by `0x8237EDB0`, stores it at context
offset `+0x04`, and later passes that exact pointer to `0x8237E4C0`. The
complete owner flow at `0x820DBF30` also initializes an optional callback/data
pointer at receiver `+0xE8`. The first receiver writes are pointer coordinates
(`+0x124/+0x128`) and a key bitset/code (`+0x12C..+0x14C`), not player or
camera state. Its next synchronous handoff uses an interface object whose
first word is `0x8205A8EC`; the `+0x28/+0x2C` entries are `0x820D99F8` and
`0x820D9A28`, both interior entries in the complete graphics shadow-state
flow. The table owner is now proven, but its entry ABI and gameplay ownership
remain unresolved, so no additional native helper was added. See
`FUNCTION_8237E4C0_EVENT_RECEIVER_REPORT.md` and
`reports/cycle-99-event-receiver-owner-initialization.md`.

The formerly opaque `0x820D9A28` target is now decomposed inside its complete
`0x820D99C0..0x820D9B38` host flow. Its `+0xAA0..+0xAEC` writes are five
`float4` values in a graphics shadow-state bank: the downstream
`0x821E24D8` consumes dirty masks, hardware-register ranges `0x2000..0x2280`,
and command-stream cursors. This is render submission state, not an owned
player/camera transform. The mismatch between the receiver callback ABI and
the interior table entry remains a runtime-table question; see
`FUNCTION_820D99C0_GPU_STATE_REPORT.md`.

The alternative logical-input consumer at `0x821B9048` is also closed by
owner identity. Its vtable locator resolves to RTTI type
`CModeTaskTitleMovie`; `0x821B9110` is only an interior bit test within that
update. Logical bits 0 or 4 can skip/advance the auxiliary title-movie object,
after which a three-tick state emits the generic global-flow tuple `{1, 3}`.
No active-aircraft, mission actor, spawn, or gameplay-camera object is obtained
or written, so this route is a typed non-gameplay blocker and no native helper
was added. See `FUNCTION_821B9048_TITLE_MOVIE_FLOW_REPORT.md`.

The 553 MOP/GYZ and 44 CUT resources now have a deterministic remaster-ready
export under `remaster-export/`, preserving raw bytes, structured tables,
provenance, relations, offsets and checksums. Three generated trees compared
byte-identical; see `SCENE_REMASTER_EXPORT_REPORT.md`.

Next, pivot from an independently typed gameplay-mode, active-aircraft, spawn,
or gameplay-camera owner and trace its mutations backward to logical input by
pointer identity. Do not continue through the title-movie consumer or promote
the GPU shadow-state table to gameplay state. A runtime object/table trace may
still determine whether the `+0xDC` interface target after `0x8237E4C0` is
replaced before dispatch. In parallel,
extend the world join across the remaining first-CUT objects, decode
MATE/NTXR presentation state, and prove the retail camera rotation/projection
order. The dynamically supplied vtable/class behind dispatch-context
first-pointer offset `+0x10` remains a bounded parity question rather than a
blocker for the now-observable native geometry.
The static initialization and vtable provenance of the type-`0x98` service
behind `0x820a7070` are now closed: `0x826a0728 -> 0x826a0708 ->
0x820674d8`, with exact `+0x18/+0x1c/+0x10/+0x24` targets. The result is
allocated/cursor-backed at `0x120`/`0x10`, stored at owner `+0x15c`, and
re-injected through service slot `+0x10`; see
`reports/cycle-136-type98-service-vtable.md` and
`reports/cycle-137-type98-result-lifecycle.md`. The remaining question is the
runtime/business identity of that result and its connection to a proven draw
submission or scene traversal anchor. Then recover the runtime pool construction, Xenos
descriptor bit fields, the inner meanings of neutral
`NDXR`/`NSXR` regions, and a `MATE` consumer. Retain the existing D3D resolution
anchors for the future resolution-independent renderer.

The immediate post-processing is also bounded: `0x822383d0/0x82238408` read a
bounded index-3 view, `0x821d65c0` performs hierarchical key lookup,
`0x822a1258/0x822a9690` copy fixed `0x60`/`0x40`-byte sub-states, and
`0x82286210` marks an auxiliary state after `0x82284e88`. See
`reports/cycle-138-type98-postprocessing-contracts.md`. The payload identity
and any draw/flight consumer remain unknown; no human session is required for
the next static step.

The caller frontier for `0x822c2148` is now closed at the instruction level.
`FindDirectCallsTo.java` finds exactly three direct sites in the same raw worker
`0x82105bb8..0x82106354`: `0x82105ccc`, `0x82105fb8` and `0x821061c0`. All use
the same ABI (`r3` three-float output, `r4` scalar output, `r5` result of the
indirect slot `context+0x00+0x5c`, `r6` low 16 bits of the table word), test the
low return byte, then quantize and clamp state values to `0..0xf`. Ghidra's
export still has no caller and does not assign the raw worker to a containing
function; this is an export/boundary correction, not a semantic identification.
See `reports/cycle-139-ndxr-caller-boundary.md`. The resource pointer, table
consumer and any draw/flight relation remain unknown; the next static step is
to resolve the indirect slot and its writers. No human session is required.

The worker entry is now reconciled as well. `FindPpcBranchesTo.java` finds
exactly two callers of the catalogued entry `0x82105ba8`, at `0x820fbbd4` and
`0x820fcf3c`; both pass the same owner register (`r31`) in `r3`. The contiguous
body starts at `0x82105bb8`, after the common PPC preamble, so the apparent
missing function boundary is an analysis artifact. Both callers dispatch a
slot at `+0x13c` through their first object word before entering the worker,
while the worker dispatches `+0x5c` through `context+0x00`. Their post-return
writes remain offset-qualified and do not prove scene, renderer or flight
semantics. See `reports/cycle-140-ndxr-worker-entry-owner.md`; the next static
target is the dynamic vtable/field provenance and slot writers. No human
session is required.

The table at `0x8205c980` is now an observed vtable candidate: an initializer
writes it at owner offset zero around `0x820f9dfc`, and its words `+0x5c` and
`+0x13c` resolve to `0x82101be0` and `0x821002f0`. The same table also contains
the two worker callers at `+0x10c` (`0x820fbc28`) and `+0x110`
(`0x820fa9c0`), a strong method-family cross-match. The two leaf contracts are
confirmed locally, but their use by the dynamic `r31` owners is not yet proved.
In particular, the worker's `rlwinm r4,r31,0x10,0x17,0x1f` produces a 9-bit
field while `0x82101be0` dereferences `r4+0x1c`; the encoding/provenance of
that value remains open. Do not yet call it a record pointer or assert that
this candidate slot is the `r5` producer at the three worker calls. See
`reports/cycle-141-ndxr-vtable-slots.md` and the qualification in
`reports/cycle-142-ndxr-vtable-provenance-qualification.md`; the next static
target is the writer/provenance chain for the context field `+0x28`, `r31` and
`r4`.

The subobject provenance is now tighter: the allocation path at `0x8212a2a8`
calls `0x820f9dc8` with `outer+0x14`, and that initializer writes
`0x8205c980` at the subobject's offset zero. The same table contains the two
worker-caller methods at `+0x10c` (`0x820fbc28`) and `+0x110` (`0x820fa9c0`),
which is a strong method-family cross-match. The outer object has a distinct
observed vtable candidate at `0x8205d6c0`; do not merge the two layouts. The
worker consumes the subobject-family field `+0x28`, but the exact table value
and the effective `+0x5c` implementation remain runtime-qualified because the
worker still forms a 9-bit `r4` field before the leaf's apparent pointer
deref. See `reports/cycle-143-ndxr-subobject-vtable-provenance.md`.

The `+0x28` field is now structurally qualified as a resource pointer table:
`0x82234e08` performs a bounded pointer-table lookup, and `0x820fa9c0` stores
its literal-index-`0xb` result into subobject `+0x28` (with a later conditional
zeroing path). The worker then uses this field as the base of its indexed word
walk. This closes the table-production shape, not the entry semantics or the
effective `+0x5c` override; the `r4` 9-bit/address discrepancy remains open.
See `reports/cycle-144-ndxr-resource-table-field28.md`.

Cycle 145 corrects the vtable distinction. The outer object at `0x8212a2a8`
uses the distinct candidate table `0x8205d6c0`; its word at `+0x5c` is
`0x820731bc`, not the worker leaf. The call to `0x820f9dc8` with `outer+0x14`
writes `0x8205c980` at the subobject offset zero, and that subobject table is
the only statically coherent one for the worker family (`+0x5c -> 0x82101be0`,
`+0x13c -> 0x821002f0`, callers at `+0x10c/+0x110`). This closes the
outer/subobject separation, not the dynamic instance provenance. The 9-bit
`r4` versus `lhz 0x1c(r4)` contradiction remains explicitly open; no pointer or
`r5` semantic is promoted. See
`reports/cycle-145-ndxr-subobject-dispatch-correction.md`.

Cycle 146 adds a second static constructor path. `0x82183960` prepares a
parent-owned subobject at `param_1+0x611` (`r31+0x1844`) and calls
`0x820f9dc8`, which again writes `0x8205c980` at subobject offset zero before
calling `0x820f9e78` and `0x822b65e8` on adjacent members. In the raw body of
`0x820fa9c0`, the local descriptor at `r1+0x50` is queried at several literal
indices; the bounded pointer-table lookup `0x82234e08(index=0xb)` is stored at
subobject `+0x28`, while neighboring indices populate `+0x0c..+0x2c`. This
strengthens the structural `resource_pointer_table` qualification without
assigning a NDXR or renderer meaning to its entries. The worker still forms
`r4=(r31>>16)&0x1ff` before its indirect dispatch, so the `r4+0x1c` leaf
contradiction remains `needs-dynamic-evidence`; no human session is required.
See `reports/cycle-146-ndxr-second-constructor-resource-lookups.md`.

Cycle 147 corrige une ambiguïté de portée entre le champ mémoire `owner+0x5c`
et le slot vtable `vtable+0x5c`. Le dump headless de `0x82101a18` montre un
résolveur de ressources qui normalise un chemin, initialise une adresse de
sortie et renvoie un handle/pointeur contrôlé; `0x82101b28` reste le
constructeur de chemin distinct. `0x820fbc28` remet plusieurs champs à zéro et
alimente `+0x0c..+0x2c` par des résolutions séparées. Pour `+0x28`, le retour
est produit avec `owner+0x5c` comme adresse de sortie, puis peut être libéré et
annulé lorsque le type vaut `4`. Cette preuve renforce la forme de résolution
de ressource sans donner de sémantique NDXR/draw/vol. La contradiction du
worker (`r4` 9 bits contre feuille lisant `r4+0x1c`) reste
`needs-dynamic-evidence`; voir `reports/cycle-147-ndxr-resource-resolver-field-separation.md`.

Cycle 148 exclut une fusion erronée avec le chemin gameplay entry-9. Les
écritures `object+0x28/+0x5c` du `X360UnitManager` utilisent sa vtable
`0x82055190` et son constructeur `0x82273880`; elles ne sont pas des writers du
sous-objet NDXR, dont le constructeur `0x820f9dc8` écrit la vtable `0x8205c980`
sur `outer+0x14` (ou sur le second parent qualifié). Les références headless
vers `0x8205c980` ne montrent que l'écriture du constructeur
(`0x820f9dfc`). Les offsets numériques communs ne permettent donc aucune
identité d'objet. La frontière worker/`r4` reste `needs-dynamic-evidence` et
aucune action humaine n'est requise; voir
`reports/cycle-148-ndxr-entry9-nonmerge.md`.

Portfolio scheduling is Pharaoh first, AC5 second and AC6 third.

## First material/texture identity boundary

The aircraft model/texture MDLP pairs `76/77` and `78/79` now connect 115 NDXR
polygon texture-id references to exact paired NTXR GIDX values. There are 227
exact occurrences of those identifiers in the four associated MATE payloads.
The first matched NTXR is now a proven six-entry wrapper. Its `0x10002215`
entry decodes natively as a 512x512 single-level BC3 atlas with Xenos tiled-2D
addressing and `8-in-16` endian conversion; exact logical consumption is
262,144 bytes. MATE batch ordinals now map exactly to NDXR polygon descriptors,
material first-texture identifiers match NDXR and NTXR, and every non-restart
NDXR index is local (`index_oob=0`). The two aircraft present 52 and 63 proved
textured polygons respectively; only two first-aircraft UV-incomplete batches
remain wireframe. The frame remains conservatively `native-partial`. See
`AC6_MATERIAL_TEXTURE_LINK_REPORT.md`.

## Canonical motion/resource separation (cycle 211, 2026-07-18)

The canonical PAL image confirms a distinct `CX360MotionRequestManager` vtable
at `0x8205cd90` (`CX360MotionRequestManager`), with slot `+0x08` dispatching
records tagged `0x11` or `0x8181`. `0x82136100` and `0x821371d8` forward
object fields `+0x1a4/+0x1a8` to this manager. A raw producer block at
`0x82127eb0..0x82127f30` fills those fields and calls `0x82136168`, but its
function boundary and object type remain unknown.

Separately, `0x82226c20`, `0x8228e9e8`, `0x8228fc80`, `0x82293d08`,
`0x82374590` and `0x82374978` consume `+0x15c` as resource/property/table
state. This offset homonym must not be merged with the unit manager or motion
manager without pointer provenance. No static edge to the shared owner
receiver, selector-1 campaign identity, `CutTerminate` consumer, or a flight
owner was recovered. AC6 remains `native-partial`, boundary `scene_complete`;
no human action is required for this tranche. See
`reports/cycle-211-canonical-motion-resource-boundary.md`.

## Canonical motion-record producer boundary (cycle 212, 2026-07-18)

The direct callers of `0x82118a50` were rechecked on the canonical PAL image.
The helper normalizes tags `0x11` and `0x8181`; raw resource-parser sites
`0x82128c90`, `0x8212a100`, `0x8212a23c` and `0x8212a598` resolve `entry+0xd0`,
store it in local fields `+0x14/+0x1c/+0x3c/+0x24`, normalize the record and
set `0x826948c0 = 1`. The raw block `0x82127f30 -> 0x82136168` remains an
unknown object initialization path writing `+0x18c..+0x1a8`; its function and
type are not recovered.

This strengthens the resource-record classification but still does not join
the motion manager to the shared owner receiver, selector-1 campaign identity,
`CutTerminate` consumer or a flight/camera owner. AC6 remains
`native-partial`, boundary `scene_complete`; no human action is required.
See `reports/cycle-212-motion-record-producer-boundary.md`.
