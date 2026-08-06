# AC6 — Native Vulkan reconstruction plan

Updated: 2026-08-04

## Current native checkpoint — 2026-08-04

The native Mission 01 boundary now has reproducible external contracts for
NDXR terrain/sky slices, PPM texture sources, SDL controls, qualified cameras,
fixed-step replay/save state, and fail-closed package auditing. The durable
slice inventory is verified by `tools/verify_mission01_slice_inventory.py`
(`rows=14`). A developer render using retail entry-119 slices reaches the
native rasterizer (`geometry=15`, `triangles=4212`, `coverage=2712`).

The parity checkpoint remains open: no positive, intervention-free oracle pack
with a `qualified` camera, complete sky transforms, and exact texture bindings
has been sealed. Black/sparse captures are diagnostic only. Do not promote
developer manifests or bridge-lane camera constants to retail evidence.

## Checkpoint cycle 783 — frontière produit native

`ac6-native` et `ac6_product_core` existent désormais comme targets séparés
des inspecteurs. Ils exposent les quatre interfaces produit, les IDs stables et
un driver à pas fixe fail-closed. Aucun composant diagnostic rejeté ni symbole
guest n'est lié. Le binaire refuse encore de démarrer Mission 01 (code 2), ce
qui est volontaire tant que le contrat oracle est incomplet.

La dérivation canonique précise aussi que LY `device+0x3E` est séparé par
`0x8234D110` vers `device+0x28` (positif) et `device+0x2A` (négatif). Le
prochain test discriminant doit instrumenter ces lecteurs et les joindre à
l'enfant joueur ; voir `reports/cycle-783-native-product-boundary.md`.

## Active checkpoint — canonical input consumer for pitch

Cycles 778–779 prove in lane `bridge` that the black Mission 01 frame still runs
`CModeTaskGame`, mission-manager updates and `UpInput/UpObj/UpCam/UpRadio`.
The live UnitManager contains one exact `CAce6UnitPlayer` among 230 objects and
raw pitch/roll/button/trigger values reach XAM. Canonical factory analysis
proves this player is a 256-byte wrapper, rejecting the former `player+10672`
join. Its vtable slot `+0x3C` is `0x822A6710`; cycle 779 observes an exact live
child `0xB2470100` through `+216/+220` and a changing transform copied into
`+144..+207`.

Cycles 780–782 reproduce an immediate transform response on the stable player
child under positive pitch only. Cycle 782 closes the canonical ingestion at
`0x8234D378`: XAM `ly=32767`, raw `device+0x4E=0x7FFF` and canonical
`device+0x3E=0x7FFF` are simultaneous, then return together to zero. The old
`0x821CE088/0x82215418/0x82215210` input roles are rejected for the canonical
project. Next: use Ghidra Bridge to find the first reader of canonical
`device+0x3E` that stores into the owned player/child chain. Do not synthesize
a controller or reuse `player+10672`. Stock/observe
confirmation of Loading→Game remains required. Rendering stays a parallel
frontier; the ordered first-black render graph is still open. See
`reports/cycle-782-canonical-pitch-input.{md,json}`.

## Objective

The product is a native Windows/Linux version of Ace Combat 6 with its own
Vulkan backend and no runtime dependency on Xbox libraries, RexGlue, Xenia or
another emulator. XenonRecomp, RexGlue and Xenia are temporary evidence and
execution tools only; none defines the product architecture or ships in the
final runtime.

Reach a reproducible, genuinely playable Mission 1 vertical slice as an
evidence checkpoint, then turn the contracts proven by that slice into native
portable subsystems that make additional missions incremental rather than a
succession of mission-specific fallbacks.

Every accepted implementation must move toward at least one final subsystem:

- portable asset and resource loading;
- native campaign, mission, objective, save and progression state;
- native input and gameplay services;
- native audio/video services;
- an AC6-owned Vulkan renderer and platform presentation layer.

Guest-address probes, XAM/xboxkrnl compatibility services, generated PPC
dispatch and RexGlue/Xenia renderer patches must remain isolated, tested
evidence adapters with an explicit replacement boundary. A checkpoint is not
architecturally closed while new game behavior exists only in one of those
adapters.

The accepted Mission 1 outcome requires:

- Campaign New Game, Normal difficulty, Normal controls and English language.
- Mission screens and loadout reached without forced guest writes.
- Correct aircraft and weapon information, including a populated capability
  polygon.
- World geometry and HUD visible.
- Pitch, roll, yaw and throttle input producing guest-state and visible changes.
- No fatal signal, unresolved indirect dispatch, embedded retail data or
  experimental mission fallback.

## Audited state

- Menus and campaign selection reach the Mission 01 hangar on the corrected
  corpus, but a controlled flight has not been demonstrated.
- The generator now preserves an exact declared function entry even when it is
  contained in another function's block range. A synthetic overlapping-entry
  fixture is registered in the SDK unit tests.
- The former runtime fatal around `0x8236BC38 -> 0x8236BC0C` is absent after
  removing the unqualified `0x8236BC30` start; the generated corpus builds and
  reaches the campaign boundary.
- Cycle 604, with both loadout force options disabled, reaches a rendered
  `MISSION 01 / STANDBY` hangar. Captures through pitch, roll, throttle and
  brake are byte-identical, so this is not yet a playable-flight checkpoint.
- Cycle 607 qualifies the natural campaign resource call at
  `0x8218F3A0`: result `1`, `mode=1`, `selector=0xFFFFFFFF`, current level `1`.
  The new stage arm only publishes a diagnostic epoch; it performs no guest
  write and no readiness/launch override.
- Cycle 609 replayed the cycle-539 profile with both force options disabled, but
  did not reach the expected `type28=30` state before timeout. Its repeated
  pulse inputs are therefore not loadout evidence.
- The aircraft capability axes render, but the polygon and related loadout data
  appear incomplete. Treat this as a data-publication/state invariant before
  attributing it to the renderer.
- Cycle 514 crossed the first PAL child record with a bounded synthetic entry.
- Cycle 517 proves that the second timeline iteration does not reuse `r6` and
  that the repeat count is read from UTF-16-like data through a bad
  `state+40` table/owner contract.
- Cycles 525–528 correct the former readiness hypothesis:
  `level_root+0x276A0` is the current controller-button bitset and `0x20` is
  Back. It is not a resource-completion publication.
- Cycles 530–532 show that the loadout state setter receives only state `0`.
- Cycles 539–543 are corrected by cycle 544: `CModeTaskGame`, the campaign
  MissionManager and ArmsManagers are downstream of accepted loadout, while
  `level_root+0x36054` is `CX360EffectManager`; effect ids 200–208 are not
  aircraft-selection events.
- Cycle 544 identifies the active object exactly as
  `CSelectAircraftManager`, embedded at `CModeTaskAircraftSelect+0x270` and
  derived from `CSwgListener`. It is naturally registered in the 16-entry
  listener table at `0x8293B800`; the missing boundary is not registration.
- Cycles 545–546 show that the generated runtime receives `M200_select_OK`,
  but its public `atoi` thunk returned 22 for every valid decimal input.
  Cycle 548 adds a bounded deterministic implementation of the exact PAL
  `0x82382480` thunk contract; all seven AC6 tests pass.
- Cycle 549 resolves the apparent Ghidra contradiction. SHA-guarded headless
  byte dumps match the generated corpus exactly; false no-return metadata and
  missing function boundaries caused the truncated catalog. The canonical
  project now persistently defines the parser at `0x820F62B0`, `SendMsgV` at
  `0x820F6330`, and the aircraft-manager dispatcher at `0x8214D390`.
- Cycle 568 proves a state-driven fresh-profile route through game-data
  creation to the Mission 01 `START MISSION` screen. The first launch exposed
  a generated control-flow defect at `0x82331D4C -> 0x82331CD0` rather than a
  loadout publication failure.
- Cycles 570–575 repair the generator's overlapping-entry classification and
  remove runtime-proven false function starts. Two independent generations are
  byte-identical (manifest SHA-256
  `c6c0b2b5cd5b6e05383a55d0da56abcdb5e950a43f2188424d6ff4b5945541c1`).
  The corrected corpus now reaches campaign setup without force flags; the
  next exact frontier is a false split around `0x822EEF28..0x822EEFC8`, first
  observed as `0x822EEF90 -> 0x822EEF5C` during the campaign transition.
- Mission instrumentation and compatibility logic are concentrated in a
  roughly 3,100-line probe source with about 55 wrappers.
- Graphics runtime status is now published under a mutex. D3D live counters and
  captured records use one frame-publication lock, eliminating counter/record
  epoch splits at the boundary.
- PAC index lookup now uses a structured archive/offset/size key and 64-bit
  interval endpoints, closing the archive-bit collision and wrapped-range
  classification defects.
- PAC read completion now copies bytes into a bounded per-archive buffer before
  reconstruction, so later buffer reuse cannot silently change an entry.
- Cycle 611 rebuilds the PAC-hardened binary and completes a bounded runtime
  smoke with 1,642 `PRESENT` lines and no fatal/assert/unresolved marker. The
  qualified binary SHA-256 is
  `b21605cdb4008be6bf62577f069ed21851b1eb5ac385e460abd4bb6c29f3c077`.
- Cycle 612 adds the PAC archive-key/range regression fixture and atomic dump
  publication. The focused AC6 suite is now 8/8; the rebuilt binary smoke
  publishes 1,655 `PRESENT` lines with no fatal/assert/unresolved marker. Its
  SHA-256 is
  `8987d3497f439aa67174f82f3774f4f16c6a841ea9ecd125733b1a0d4f6911ad`.
- Cycle 616 is a bounded negative title/video checkpoint: Escape/confirm and
  XAM button edges are observed, but the intro frame remains unchanged through
  225 seconds and no selector, save or loadout hook fires. Repeating this timing
  recipe would not add evidence.
- Cycle 617 re-runs the canonical PAL Ghidra repair with SHA-guarded byte ranges
  for `0x8218C238..0x8218CCA4` and `0x821C37E0..0x821C3BE0`. It also clears the
  false no-return/`CALL_RETURN` metadata on savegprlr r14/r22/r29. A fresh
  8,827-function export now exposes the campaign-save registration path and
  dialog state machine. This improves static guidance; it does not prove HUD
  entry or flight.
- The D3D epoch correction now uses a consistent shadow-to-capture lock order
  in draw/clear/resolve hooks and the frame boundary, avoiding an inversion
  between state setters and capture snapshots.
- Cycle 618 rebuilds that lock-order correction: AC6 CTest remains 8/8 and a
  bounded runtime smoke produces 1,659 `PRESENT` lines with no fatal/assert/
  unresolved/deadlock marker. The new binary SHA-256 is
  `36ca874d6eecbcdab4dcd3fdbc60acd06187ecc913082d5e928d70b73cc223c9`.
- Cycle 619 closes the cycle-616 timing hypothesis: the title becomes visible
  near 40 seconds, and the natural Game Data prompt is present by 50 seconds;
  Escape maps to Start (`0x0010`) and space maps to A (`0x1000`).
- Cycle 620 confirms a state-driven affirmative Game Data route: A on the
  visible prompt produces `type28=30 -> 37` and an asynchronous save task,
  rather than a guessed wall-time transition.
- Cycle 621 reached `type28=37` but exposed a harness bug that searched only
  the current log after rotation; cycle 622's wall-time replay stayed in the
  intro, confirming that fixed timing is not an acceptance predicate. The
  harness now searches rotated logs.
- Cycle 624 reaches the affirmative route through `type28=37 -> 35`,
  `selector44=3`, and file-create `type28=6`; it creates bounded Game Data
  files under the isolated profile. The route then reaches `type28=9` and
  `type28=5`, so no type-10/loadout transition is yet proven.
- Cycle 625 replays an existing profile and reaches the same type-9 loop. This
  keeps save-file validity/task results in scope; it is not evidence for a
  force flag or a renderer defect.
- Cycle 626 byte-qualifies and repairs the four long save helpers in the
  canonical Ghidra project: `0x821C3BE8`, `0x821C4FA0`, `0x821C5258` and
  `0x821C56F8`. Fresh decompilation exposes the selector's separate
  `[screen+0x10]` selection and `[screen+0x0C]` response epochs, the async
  result branch that can legitimately lead to type 9, and the storage state
  machine. No generated output was edited.
- Cycles 639--642 remove the obsolete first-campaign bridge marker from the
  native-readiness replay and let the focused loadout trace publish the
  first-mission task marker without enabling the timing-heavy UI dispatcher
  trace. They also prove that the shared user-data root changes the save entry
  state; future acceptance replays require an isolated, recorded user-data
  root. These cycles qualify the harness, not native readiness.
- Cycle 643 integrates the targeted-asset audit: raw PAC rows are XOR-padded
  despite their storage flag (126/126 decode to FHM), row 9 is qualified as
  Mission 1 content, and cycle 638's PAC reads map uniquely to 55 table rows
  with transition tail `199 -> 9 -> 119 -> 165 -> 210`. This strongly
  corroborates, but does not directly prove, DPL id 9 -> table row 9.
- Cycle 675 reaches the complete no-force Mission 1 route with a compact
  Vulkan pass catalog. The hangar aircraft and cinematic terrain are textured,
  but the cinematic aircraft meshes are white. They submit geometry and
  non-null texture descriptors (`D5B4F4A878949938`, bases `0x06B30000` and
  `0x045FB000`), so the remaining cutscene defect is a bounded material/view,
  fetch or shader-semantic mismatch rather than missing geometry.
- Cycle 674's direct-host-resolve A/B remains negative for the black gameplay
  world. Cycle 676's enriched-volume catalog stalled at 943 presents before
  campaign entry and is instrumentation-only; do not treat it as graphics
  evidence or repeat the full route unchanged.
- Cycle 679 reaches the cutscene and flight HUD with the bounded material-view
  diagnostic. The white-aircraft `D5B4F4A878949938` path selects a non-null
  unsigned BC3 view for both shader signedness requests (`guest_format=20`,
  `host_unsigned=137`), just as the known-good hangar `C441...` path selects
  its unsigned BC1 view. Cycle 675 also shows the same tiled/endian/swizzle
  BC3 path working for hangar/sky surfaces. The signed/unsigned and generic
  BC3-path hypotheses are therefore deprioritized; the next renderer split is
  cutscene material content/mip selection versus UV/constant/shader semantics.
- Cycle 680 dumps 244 qualified ucode shaders. `D5B4...` has one explicit
  `tfetch2D` from interpolated `r0.xy`/`tf0`; its two cutscene vertex families
  fetch UVs at attribute offset 6 and export `o0.xy`. The static path is not
  textureless; only runtime sample content/mip choice or post-sample lighting
  remains unresolved. The replay itself stalled before `type28=30` and is not
  gameplay evidence.
- Cycle 681 reaches the same Mission 01 cinematic and flight captures with
  D5B4/tf0 replaced by zero. The aircraft remain white and the world remains
  black, so sample content/mip alone is not causal. The portable tree now
  exposes a fail-closed `VulkanMaterialBinding` contract for the qualified
  MATE/NDXR/NTXR + shader metadata path; the next oracle is constants/final
  pixel output, not another generic texture-view A/B.
- Cycle 682 implements the off-by-default D5B4 constant hook and a bounded SDL
  focus retry, but all launch attempts stop before a D5B4 draw. Preserve them
  as harness-only; the next replay must begin at an existing-save/scene-window
  gate rather than consuming another full fresh-profile route.
- Cycle 683 qualifies that gate attempt: the only locally available cycle-675
  profile is a new-game template whose three save slots are empty (`MISSION
  ----`). It reaches `selector44=3`/`type28=6` but cannot provide a populated
  campaign checkpoint, so it emits zero D5B4 constant records and is not
  graphics evidence. The runner's state waits now consume only lines after an
  append-only follow-log baseline; see
  `reports/cycle-683-d5b4-existing-save-window.md`.
- Cycle 684 adds the native generic campaign progression contract. It resolves
  every mission spec through DPL→DATA.TBL, validates loadout identity and
  objectives, unlocks prerequisite missions and emits a deterministic save
  snapshot. Its two-mission test is synthetic and makes no retail Mission 2
  claim; see `reports/cycle-684-native-campaign-progression-contract.md`.
- Twenty Mission 1 recipes duplicate substantial menu navigation and depend on
  timing rather than strong state predicates.
- Mission selection remains hardcoded around Mission 1 instead of being driven
  by qualified mission metadata.
- The native worktree contains a large, unpartitioned diff, changed binaries
  and untracked modules; it is not yet a reviewable reproducible baseline.

Current validation baseline:

- AC6-specific CTest: 8/8 passing; the overlapping-function graph fixture and
  the PAC archive-key/range fixture are included in the focused checks.
- Full CTest run: 1,621 tests, with the same six known failures and four
  intentional skips as the prior baseline: four NT-epoch/calendar cases,
  `vpkd3d128_float16_4_invalid_0`, and an unrelated TemplateRegistry count
  expecting 11 ids while 15 are registered. No new AC6 failure was introduced.

## Checkpoint 0 — Reproducible baseline

Actions:

1. Keep the canonical handoff synchronized with the qualified cycle-544
   frontier.
2. Partition the native changes into reviewable commits:
   - deterministic recompilation fixes;
   - platform/runtime support;
   - AC6 functional bridges;
   - probes and diagnostics;
   - tests.
3. Exclude generated binaries, caches and execution artefacts from functional
   changes.
4. Record the PAL XEX SHA-256, module, build profile, source revisions and exact
   scenario with every accepted capture.
5. Provide distinct normal, instrumented and experimental-fallback profiles.

Success criterion:

- A clean checkout reproduces the current qualified loadout frontier with one documented
  command and no unexplained local state.

Immediate frontier: close the cutscene-aircraft material contract and the
black gameplay world independently. First compare the cutscene `D5B4...`
material family against the known-good hangar family with a bounded signed/
unsigned-view and host-format diagnostic. Then address the gameplay target/
resolve/world pass. Do not restore force flags, synthesize a guest event, or
spend another full oracle run before a named renderer hypothesis is built.

## Checkpoint 1 — Establish the real loadout contract

The empty capability polygon and stalled launch share an incomplete loadout
lifecycle boundary. Their exact causal relation still requires proof.

Actions:

1. Trace the aircraft and weapon records feeding the loadout panel.
2. Verify aircraft identity, capability values, weapon quantities and selected
   indices before launch.
3. Preserve the cycle-517 register capture: second-iteration `r6=0` invalidates
   the former reuse/producer hypothesis.
4. Preserve the input mapping proving `level_root+0x276A0` is the button
   bitset and the cycle-549 SHA-qualified Ghidra boundary repair.
5. Join the already executed manager setup `sub_821461C0` to the aircraft-stat
   records feeding the capability polygon and weapon quantities.
6. Use the cycle-607 level-1 resource observation to locate the native
   readiness publication, then remove `ac6_force_loadout_ready` and
   `ac6_force_loadout_launch` from the acceptance profile.
7. Add fixture tests for aircraft publication, loadout readiness and launch
   eligibility.

Success criterion:

- The selected F-16C has populated stats and weapons, its capability polygon is
  plausible, and Mission 1 launches with both force options disabled.

## Checkpoint 2 — Replace timeline fallbacks with contracts

Extract the active frontier into focused components:

- `ac6_loadout_state`
- `ac6_campaign_transition`
- `ac6_mission_timeline`
- `ac6_mission_dispatch`
- `ac6_probe_logging`

Centralize and test:

- guest pointer and range validation;
- record type and table bounds;
- data-versus-code target validation;
- timeline initialization and ownership;
- bounded diagnostics and fallback counters.

Do not extend the sparse synthetic child table, reinterpret child `+8` as an
index, retry `0x82019E6C`, or return arbitrary inputs from missing dispatchers.
Reconstruct the real record owner/type-map initialization using project-qualified

Success criterion:

- Both cycle-515 secondary dispatches resolve to qualified executable targets,
  and no synthetic timeline fallback is exercised.

## Checkpoint 3 — First playable vertical slice

Required validation:

1. World geometry and HUD are visible.
2. The mission remains stable for at least 1,800 frames after the first HUD
   frame is visible.
3. Named pitch, roll, yaw and throttle inputs cause both a guest-state delta and
   a visible response.
4. Pause, restart and abort paths work.
5. There are no forced guest writes, mission-dispatch fallbacks, fatal signals
   or unresolved indirect targets.

Success criterion:

- A fresh profile enters Mission 1 and permits controlled flight three
  consecutive times.

## Checkpoint 4 — Harden the vertical slice

Actions:

1. Replace duplicated recipes with reusable scenario stages and state-based
   waits.
2. Add regression fixtures for campaign selection, loadout publication,
   timeline creation, dispatch validation, HUD entry and flight input.
3. Require fallback counters to be zero in the normal acceptance profile.
4. Run targeted AC6 tests and the complete CTest suite.
5. Track the current five suite failures explicitly until fixed; do not hide
   new failures in that baseline.
6. Verify that normal and instrumented builds reach the same guest state, with
   probes affecting diagnostics only.

Success criterion:

- The vertical slice is repeatable across clean runs and build profiles, and a
  regression fails at the contract that introduced it.

## Checkpoint 5 — Generalize for subsequent missions

Introduce a binary-qualified mission manifest instead of `level == 0` policy.
Each mission entry should record:

- mission identifier and PAC/package index;
- required members and hashes;
- resource/container structure;
- supported lifecycle stages;
- required runtime services and explicit gaps.

Validate the generalized path against:

1. Mission 1 as the golden vertical slice.
2. One structurally different early mission.
3. One late mission stressing different resources or scripting.

Success criterion:

- Adding a mission primarily requires qualified manifest data and bounded
  service implementation, not new mission-specific guest-memory fallbacks.

## Engineering invariants

- Target Xbox 360 PAL AC6 only; preserve Xenon big-endian PPC and Xenos
  semantics.
- Qualify binary evidence by canonical Ghidra project, target, XEX SHA-256,
  module and address.
- Never edit generated recompilation output.
- Keep all guest-memory magic inside reviewed, tested contracts.
- Reject data addresses used as executable dispatch targets.
- Do not copy or embed retail containers; use bounded, hashed slices when
  evidence bytes are required.
- Use Xenia only as an observation oracle, never as parity evidence.
- Extract only seams touched by the current frontier before the playable slice;
  defer broad cleanup until the vertical path is proven.

## Immediate next action

Resume at the native contract boundary without spending another oracle on the same
profile. Cycle 681 already rejected D5B4/tf0 sample-zero; cycle 682 stopped
before a D5B4 draw; cycle 683 proves that the available copied profile has no
populated campaign slot. Cycle 685 now provides the bounded
`CampaignResourceRoute` → PAC slice → decode contract and cycle 686 gates it
with physical manifest fields; the next runtime request therefore requires a
manifested non-empty campaign save (or a controlled save-state/scene window),
not another fresh route. The generic runner rejects stale state lines by using
an append-only follow-log baseline. Cycle 687 routes the SDL shell through the
same bounded loader. The transactional `CampaignRuntimeState` now joins
selection, resource availability and progression; next attach external
XEX/DATA.TBL/PAC hashes and bounded slices to the manifest, then feed its
payloads/events to the AC6-owned Vulkan renderer only after MATE/NDXR/NTXR
identity, UV, mip and view availability are qualified. Cycles 689/691/692 now
supply fail-closed frame/lifetime boundaries, a headless Vulkan
acquire/release/submit path, NTXR RGBA8 image upload, descriptor binding, a
clear target, deterministic RGBA8 readback, descriptor-free and textured
SPIR-V triangle draws, native BC1/BC3 block upload and Xenos tiled/endian block
preservation; the next backend step is qualified AC6 shader/mesh layouts, mips
and presentation, without adding another mission-specific branch.
The static selector-2 route has also been inspected against the real PAL
corpus: DPL id 10 resolves to DATA.TBL[10], whose bounded payload is a valid
FHM with a 27-element MDLP, 40 NTXR, 71 NDXR and 127 MATE records. This remains
a resource candidate only; the empty save and absent interactive Mission 2
replay prevent any runtime/progression claim.
Cycle 702 adds a file-backed `CampaignPacBankSource` path and an optional
`AC6_ASSET_ROOT` integration test. With the local PAL corpus it validates the
same generic runtime for selector 1/entry 9 and selector 2/entry 10 using two
exact reads from `DATA00.PAC` and no whole-archive load; CTest is 52/52. This is
physical/native pipeline evidence only, not retail gameplay or save-unlock
evidence. Keep the next backend work on qualified mips/layouts and Vulkan
presentation, with the interactive Mission 2 gate still requiring a non-empty
retail save or an equally qualified state boundary.
Cycle 703 implements the generic mip transport: optional complete levels,
fail-closed extent/format/size checks, multi-level staging and Vulkan image
layouts. The synthetic 8×8→1×1 proof passes, while retail Xenos mip fields stay
unqualified and the NTXR decoder leaves the chain empty. Continue with
qualified AC6 shader/mesh layouts and presentation, not Mission 2-specific
exceptions.
Cycle 704 adds the bounded `CampaignMesh` CPU contract and feeds it from the
real NDXR payloads reached by DATA.TBL[9]/[10]: 42 and 8 valid textured mesh
records respectively. Primitive flags remain opaque, so the next implementation
must add a qualified GPU vertex/index draw seam before claiming world/aircraft
rendering.
Cycle 705 adds that bounded indexed GPU seam for the already-qualified
position.xy/UV fixture layout: two-buffer upload, explicit triangle-list
validation and `vkCmdDrawIndexed`. The NDXR z coordinate, primitive semantics,
depth and AC6 shader input remain unqualified; continue toward a real 3D shader
layout and presentation without turning this fixture into a retail claim.
Cycle 706 adds a separate `create_mesh_pipeline` with a generated
`vec3 position + vec2 UV` shader and stride-20 vertex input. The indexed quad
consumes `z` in `gl_Position`; this is the renderer seam needed before connecting
NDXR meshes, but it is not yet the retail Xenos layout or a presented mission.
Cycle 707 connects `CampaignMesh` directly to that GPU seam and proves the
CPU-mesh → staging → indexed draw → readback path. Do not call it world parity:
camera/matrix transforms, primitive flags, MATE texture selection, depth and
swapchain presentation remain the next qualified renderer contracts.
Cycle 708 adds the native camera/matrix contract: a bounded TCAM state produces
explicit row-major view/projection matrices and `CampaignMesh` vertices become
clip-space vertices with real `w`, Vulkan depth `[0,1]`, UVs and resource
metadata preserved. Invalid camera/viewport/clip/mesh inputs reject
deterministically. CTest with the PAL corpus is 54/54. This is still a native
projection seam, not proof of Xenos rotation packing, MATE binding, depth
presentation or interactive missions.
Cycle 709 adds the matching `vec4 clip + vec2 UV` Vulkan pipeline and a
`VK_FORMAT_D32_SFLOAT` render-target variant. A projected retail-shaped mesh
passes through clip upload, indexed draw and color readback on both color-only
and color+depth targets; CTest remains 54/54. The target is still headless and
each draw clears its pass, so multi-mesh occlusion, swapchain presentation,
retail shader semantics and MATE→NTXR identity remain open.
Cycle 710 adds one-pass batched clip meshes with bounded uint16 index rebasing;
the depth fixture distinguishes a far red triangle from a near green triangle
at the same pixel. The PAL asset gate records 42 textured meshes on entry 9
and 8 on entry 10. Adjacent model/texture elements now produce 1,885 and 294
matching first-texture MATE/NDXR links, and the native material resolver
accepts 1,021 and 85 bindings after actual BC1/BC3 NTXR decode. The shader
contract supplied to that resolver is still synthetic, so shader identity and
unique runtime material selection remain open. CTest remains 54/54.
The next bounded action is to pair each accepted `VulkanMaterialBinding` with
the actual shader contract/hash and texture descriptor for one retail model,
then add a presentation-facing frame boundary. This can be validated from the
local PAL corpus and shader cache without another interactive oracle; the
non-empty save and Mission 1/Mission 2 runtime gates remain separate.
Cycle 711 adds the upload-side identity guard: GIDX, extent, BC format and
resident-mip contract are checked before any Vulkan allocation, with a
synthetic mismatch rejection and a successful descriptor/lifetime path.
Keep evidence hooks off by default, never restore force flags, and keep the
black gameplay target/world pass as a separate named boundary.
Cycle 712 makes the first qualified retail shader contract explicit:
`A1863AF658456A14` / `D5B4F4A878949938`, `tf0`, UV offset 6, interpolated UV
and bounded mip range. `draw_campaign_vulkan_frame` now validates that contract,
uploads the matching NTXR, binds its descriptor, submits a clip-space indexed
batch and releases temporary handles. The SPIR-V remains caller-owned and is
not claimed byte-identical to Xenos. CTest remains 54/54. Next, keep the frame
alive across objects (no implicit clear), then attach presentation; Mission 1
HUD/controls and the save/Mission 2 gates remain unqualified.
Cycle 713 adds persistent color/depth targets with `LOAD` attachments. Their
initial clear is explicit through image transitions and `vkCmdClear*`; draws
before that clear are rejected, and two separate submissions remain visible in
one readback. CTest remains 54/54. The Vulkan surface, multi-frame
synchronization, HUD and Mission 1 remain the next boundaries.
Cycle 714 removes the D5B4 branch from frame submission: a caller-owned
qualified shader catalog is now mandatory, and the retail asset gate uses the
actual A1863…/D5B4… contract rather than zero hashes. Entry 9/10 still resolve
1,021/85 native bindings. Mission 2 can extend the catalog, while unique
runtime selection and presentation remain open.
Cycle 715 adds the explicit Vulkan surface/swapchain path and present-queue
selection. The local headless surface is created, but the current driver
returns `VK_ERROR_INITIALIZATION_FAILED` for swapchain creation; the test
records this as a controlled skip. No presentation parity is claimed. Continue
with a portable SDL/native surface route and keep HUD/Mission 1 gates separate.
Cycle 716 adds the generic HUD/input frame contract: qualified XInput mapping,
edge masks, route/loadout/objective progress, and attachment to
`CampaignVulkanFrame`. CTest is 55/55. It is data proof only; glyph/reticule
draw, presentation and Mission 1 gameplay remain open.
Cycle 717 adds the first renderer-facing HUD overlay: a persistent target,
same-pass untextured pipeline, bounded objective progress and action
indicators are read back after two prior scene submissions. CTest remains
55/55. This is a native geometry seam, not retail HUD parity; presentation,
Mission 1 gameplay/save and Mission 2 remain open.
Cycle 718 adds a platform-surface callback and SDL3 smoke target. The backend
now owns the instance→surface→present-queue lifetime and consumes the surface
through the same swapchain path as headless presentation. CTest PAL is 56/56;
the local dummy driver skips window creation explicitly. The real campaign
frame is still not routed into this target, so Mission 1/Mission 2 remain open.
Cycle 719 adds `draw_campaign_vulkan_frame_with_hud`: the qualified textured
world batch and `CampaignVulkanFrame.hud` now share one persistent target and
render-pass contract. Targeted Vulkan tests pass; presentation/interactive
Mission 1 and the save/Mission 2 gates remain open.
Cycle 720 removes the synthetic asset from that frame: the generic
FHM/MDLP→NDXR/MATE→NTXR adapter resolves real renderables for DATA.TBL entries
9 and 10, expands the qualified AC6 strip topology, and uploads/rasterizes
both with the HUD. CTest PAL is 58/58. The test framing is diagnostic AABB;
TCAM/STANDBY presentation, save and interactive Mission 1 remain open.
Cycle 721 replaces the selector-1 AABB framing with a real Scene replay:
`campaign_scene_frame` verifies the CUT initial-camera contract, resolves
`Tcam__c01.mop`, joins 16 frame-local Rigid/AnimRigid transforms, and applies
the qualified retail `-Z` camera sign before Vulkan projection. The real mesh
and HUD submit; the first mesh is sub-pixel at 128x128 (`scene_changed=0`), so
no non-black world claim is made. Selector 2 keeps the AABB smoke because it
has no Scene/TCAM group. Targeted CTest is 5/5; STANDBY, swapchain, save and
interactive Mission 1 remain open. See
`reports/cycle-721-tcam-cut-to-vulkan.md`.
Cycle 722 adds the generic runtime phase `idle → loadout → briefing →
standby → active → complete`. The retail Vulkan fixture now builds its frame
from a manifest-qualified `CampaignRuntimeState`, submits a real STANDBY frame,
rejects launch without the A edge, accepts the edge through
`launch_campaign_runtime_standby`, then submits the active frame. Selector 2
uses the same loader/frontend path. Targeted CTest is 6/6; interactive flight,
normal-resolution world visibility, swapchain, save and Mission 2 remain open.
See `reports/cycle-722-standby-runtime-to-vulkan.md`.

Cycle 723 replaces the selector-1 mesh unique by a Scene asset batch. The
retail strip converter now recomputes its bounded index count after removing
`0xffff` restarts; the named asset join exposes all qualified NDXR/MATE/NTXR
polygon parts. Assets without a qualified diffuse join are exposed as
geometry-only and submitted through a descriptor-free solid clip pipeline.
The PAL fixture reaches 115 textured parts, 91 solid geometry parts and
`scene_changed=3` at 128x128 after the real STANDBY→A gate. Targeted CTest is
4/4 and full PAL CTest is 59/59. This is still headless renderer evidence;
interactive flight, swapchain presentation, save and Mission 2 remain open.
See `reports/cycle-723-world-batch-and-solid-fallback.md`.

Cycle 724 adds a native, renderer-independent flight state contract. Normalized
pitch/roll/yaw/throttle/brake axes advance a bounded local pose and project it
through the qualified PAL TCAM camera; the fixture observes a changed flight
projection (`flight_changed=1`) after the real STANDBY→A gate. This is a native
control/projection seam, not recovered retail flight physics or an SDL event
loop.

The same fixture now completes Mission 1's two objective bits, emits a
versioned big-endian `AC6S` progression snapshot, decodes it transactionally,
restores a fresh runtime and verifies Mission 2 is available and selectable
through the same manifest/PAC loader. The PAL result is `save_bytes=28` and
`mission2_restored=1`; selector 2 loads entry 10 without a mission-specific
renderer branch. This is a native save contract and progression proof, not
retail save-file compatibility.

Targeted CTest is 6/6 (2.31 s) and the complete PAL suite is 61/61 (63.82 s).
The headless renderer still has no non-dummy presented swapchain, and Mission
2 has not yet been flown or completed. See
`reports/cycle-724-flight-controls-and-save-restore.md`.

Cycle 725 wraps `AC6S` in a bounded native file contract. The writer closes a
temporary sibling before replacing the target with the platform atomic replace
primitive; the reader rejects paths above 16 MiB and exposes typed I/O/decode
errors. The PAL fixture now writes and reads the Mission 1 snapshot from disk,
restores a fresh runtime and selects Mission 2 (`save_file_restored=1`), while
the same loader/frontend and renderer contracts remain in use. Targeted CTest
is 7/7 and full PAL CTest is 62/62. Power-loss durability, retail save
compatibility and interactive Mission 2 are still unclaimed. See
`reports/cycle-725-file-backed-save-checkpoint.md`.

Cycle 726 sends a manifest-qualified Mission 1 frame through an SDL-created
Vulkan surface and a real swapchain. Under Xvfb/X11, the PAL entry 9 frame
passes STANDBY/A, submits its texture and HUD, and is presented with
the TCAM/CUT frame batch (`scene_draw_groups=3`), `scene_changed=4439`,
`world_changed=11` and `hud_green=4439`. The deterministic CTest path keeps
the dummy-driver case as one explicit skip; the non-dummy command is recorded
in `reports/cycle-726-pal-campaign-sdl-vulkan-presentation.md`. The test uses
the actual frame-0 camera/transforms, so the remaining visibility gap is now
cadrage/échelle/depth/materials rather than surface creation; interactive axes
remain open.

Cycle 727 branche les événements SDL `GAMEPAD_AXIS_MOTION` sur le contrat
`CampaignFlightHostAxes`, les normalise sans dépendance SDL dans
`CampaignFlightInput`, puis avance huit pas natifs et reprojette le mesh TCAM;
`flight_changed=1` sous Xvfb confirme le raccord événement → état → projection.
La fixture termine Mission 1, écrit/relit le snapshot `AC6S` de 28 octets,
restaure un runtime neuf et déverrouille Mission 2. Entry 10 est ensuite
sélectionnée avec le même manifest/PAC loader, et sa frame diagnostique ainsi
que son HUD sont présentés sur la même swapchain (`mission2_presented=1`,
`mission2_changed=6974`, `mission2_hud_green=4428`). Le readback de vol du
premier groupe texturé reste nul (`flight_world_pixels=0`) malgré la projection
changée; ce résultat négatif borne la prochaine correction aux matériaux,
alpha, topologie et profondeur.

Le targeted CTest est 8/8 avec un skip dummy contrôlé; la suite PAL est 63/63
avec le même skip. Voir
`reports/cycle-727-flight-axis-mission2-presented.md`.

Cycle 728 remplace la complétion directe par `CampaignRuntimeEvent` et
`apply_campaign_runtime_event()`. Le dispatch borne les types objectif/mission,
rejette une complétion prématurée sans mutation et impose les objectifs requis
avant le passage `active → complete`. La fixture SDL utilise ce contrat pour
Mission 1; le résultat présenté de Mission 2 reste inchangé. CTest ciblé : 4/4
avec un skip dummy; CTest PAL complet : 63/63 avec le même skip. Voir
`reports/cycle-728-generic-runtime-events.md`.

Cycle 729 reprojette désormais tous les meshes géométriques Scene après la
pose de vol, puis les soumet au pipeline solide générique en plus du mesh
texturé diagnostique. Sous Xvfb, la projection change et le readback atteint
`flight_world_pixels=12` hors HUD (`flight_pixels=4440`). La couverture monde
de la pose est donc rasterisée; la faible couverture texturée (`textured_changed=1`)
reste une frontière matériaux/alpha, pas une preuve de parité retail. CTest
ciblé : 5/5 avec un skip dummy; CTest PAL complet : 63/63 avec le même skip.
Voir `reports/cycle-729-flight-world-coverage.md`.

Cycle 730 ajoute une pause de harness optionnelle après `vkQueuePresentKHR` et
réalise une capture X11 réellement post-présentation. La fenêtre
`AC6 campaign Vulkan frame` reste entièrement noire (307 200 pixels noirs)
alors que le readback Vulkan de la même exécution est positif. La frontière
surface/swapchain est donc maintenant séparée en deux contrats : soumission et
readback validés, composition/visibilité écran non validée. Voir
`reports/cycle-730-x11-screencap-black.md`.

Cycle 731 corrige la cause de cette capture noire : les fenêtres SDL des smoke
tests restaient `SDL_WINDOW_HIDDEN`. `SDL_ShowWindow()` est désormais appelé
avant la surface, et le backend préfère le format RGBA8 identique aux cibles
de copie. La capture post-présentation affiche l'avion, le monde et le HUD
(`screenshot-2026-08-04_03-06-00.png`); CTest ciblé 4/4 et complet 63/63.
La couverture texturée reste partielle, mais la visibilité écran est fermée.
Voir `reports/cycle-731-visible-sdl-vulkan-screencap.md`.

Cycle 732 requalifie cette capture : elle est visible, mais ne constitue pas
une frame de gameplay. Le HUD vert reste un overlay diagnostique, l'orientation
de l'avion vient du mesh Scene de la cinématique, et Mission 2 utilise encore
un mesh ajusté artificiellement au clip. Aucun skybox ni terrain n'est fourni
par le groupe Scene chargé. La projection ajoute maintenant un clipping
triangle-par-triangle au plan proche, sans modifier l'ancien contrat strict;
le CTest de projection confirme le cas avec un sommet derrière la caméra,
mais les métriques restent `world_changed=11`, `textured_changed=1` et
`flight_world_pixels=12`. Un inventaire metadata-only de l'entry 9 recense
292 NDXR et quatre candidats `mapobj` statiques; ils doivent être rejoints au
batch/runtime transform avant toute nouvelle capture. Voir
`reports/cycle-732-graphical-frontier.md`.

Cycle 733 prépare la reprise de session vers une Mission 01 native jouable
1:1. Le handoff fixe les identités PAL, la frontière graphique négative, les
fichiers à préserver, les validations, cinq checkpoints et les contraintes
anti-faux-positifs. Les anciens pipelines `ac6-scene-shell` et Xvfb `:98` ont
été arrêtés par SIGTERM après autorisation explicite; le Xvfb Pharaoh `:106`
est resté intact. Voir `reports/cycle-733-session-restart-handoff.md`.

## Prochaine frontière vérifiable

1. Rejoindre les quatre candidats `mapobj` de l'entry 9 au transform/batch
   runtime et mesurer séparément leur couverture avant de modifier le HUD.
2. Qualifier la ressource sky/cloud et la caméra/pose de gameplay; ne publier
   aucune skybox, terrain ou orientation synthétique sans preuve asset/runtime.
3. Remplacer le mesh diagnostique de Mission 2 par un frame Scene/TCAM ou
   marquer explicitement l'absence de frame; aucune capture ne doit le présenter
   comme gameplay.
4. Refaire le HUD à partir de constantes/assets qualifiés, puis remplacer
   progressivement le fallback solide par les contrats matériaux, profondeur
   et shaders; aucune branche par mission ni force flag dans le renderer.

## Residual risks

- The current native diff can conceal dependencies on untracked state.
- `origin` now publishes clean baseline snapshot `44602b8b...` on
  `ac6-first-mission-audit`; its tree exactly matches local clean HEAD tree
  `e28f47e0...` and excludes the dirty worktree. It is intentionally a root
  commit because the local historical object store is incomplete. GitHub also
  reports the baseline `.gitmodules` blob as malformed (it contains a UTF-8
  BOM); repair that in a reviewed follow-up without rebasing the snapshot or
  folding in the unpartitioned changes.
- Synthetic timeline continuation may have moved the observed failure beyond
  its real root cause.
- Resource loading and listener registration are proven, but the external SWG
  message producer and aircraft-stat publication remain unjoined. The precise
  relation between the empty polygon and the stalled acceptance state is still
  unproven.
- The new TemplateRegistry count failure is outside AC6 and must remain
  separate from loadout changes.
- Full-suite calendar and PPC failures remain unresolved and must be kept
  separate from new AC6 regressions.

## Checkpoint graphique cycle 760

Les quatre `mapobj_m01` de l'entry 9 passent le raccord exact
MATE→NDXR→NTXR. Les variantes normales réellement exécutées, `brg1_n` et
`brg2_n`, sont reliées à leurs vertex fetches, textures Vulkan non nulles et
constantes VS object-to-clip de gameplay. Le décodeur accepte uniquement leur
profil NTXR 24 mots exact.

Deux runs SDL/X11 Vulkan identiques font passer la couverture hors HUD de 12 à
141 pixels, dont 129 attribuables au batch. Les variantes `_b` restent hors
affichage faute de draw runtime. Build réussi; tests discriminants 3/3; CTest
PAL 63/63 avec le skip SDL dummy attendu. Voir
`reports/cycle-760-qualified-entry9-mapobj-batch.md`.

Prochaine frontière : imposer et relire `ac6_unlock_fps=false` au prochain run
runtime, puis identifier terrain, sky/cloud, avion joueur/LOD et caméra de vol,
dans cet ordre. Aucun asset, transform, HUD ou cadrage synthétique ne doit
combler ces absences.

## Checkpoint runtime stock cycle 774

Le run frais Mission 01 relit `ac6_unlock_fps=false`; les hooks delta, present
et flip restent inactifs. Une frame GPU retardée de 1 800 frames après le
premier draw `mapobj`, puis capturée après le HUD et les entrées `w/d/q` et
throttle, contient 1 340 draws. Le join exact ferme le terrain entry 119
(`mapparts_m01_*`), le paquet sky/cloud `entry119/022_FHM` et les matrices de
caméra de vol c218–c221. Ce n'est ni une caméra CUT ni Mission 2.

Le F-16 joueur est identifié statiquement dans l'entry 9, LOD1–4 et modèle
détaillé, mais aucun de ses vertex hashes exacts ne joint cette frame : cette
frontière reste ouverte. Le draw d'avion blanc sélectionné possède un fetch
valide et une image/view Vulkan non nulle, ce qui exclut seulement la classe A.
Le monde présenté reste noir avec HUD visible; le premier étage noir n'est pas
encore connu. Build runtime et reconstruction réussis, CTest PAL 63/63. Voir
`reports/cycle-774-stock-gameplay-terrain-sky-camera.md`.

Prochaine frontière : joindre l'index buffer du F-16 à un LOD runtime exact,
puis tracer clear/draw/RT/resolve/swap jusqu'au premier étage noir. L'A/B
`ac6_fix_deswizzle=false` de l'avion blanc vient ensuite ; aucun fetch global
unsafe, asset de remplacement, terrain/sky/HUD ou transform synthétique.
