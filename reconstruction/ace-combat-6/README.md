# AC6 native Linux — developer preview

This package contains only the native Linux runtime boundary and headers. It
does **not** contain retail archives, extracted assets, or an assertion of
frame parity with the PAL disc.

The PAC/DATA archives are not runtime inputs and are not required by the
Mission 01 comparison lane. That lane consumes only bounded, content-addressed
buffers plus a local oracle reference pack; neither is installed by CPack.

## Mission 01 comparison gate

Run:

```sh
ac6-native --compare-mission01 MISSION_MANIFEST 1 REFERENCE_DIR OUTPUT_DIR
```

`REFERENCE_DIR` is fail-closed and contains exactly the qualified evidence for
the 30-second slice:

- `replay.ac6rply`: 1,800 fixed-rate input frames;
- `checkpoints.tsv`: five ordered player/camera checkpoints ending at tick 1800;
- `oracle-color.ppm`: positive 1280x720 oracle color readback;
- `oracle-depth.f32`: 1280x720 normalized float depth readback;
- `reference.tsv`: version, mission, dimensions and FNV-64 identity of every
  file above.

Seal a newly captured pack before running the native comparison:

```sh
python3 tools/seal_mission01_reference.py REFERENCE_DIR
```

The sealer rejects wrong dimensions, frame counts, checkpoint ticks, invalid
depth values and missing files before writing `reference.tsv`.

For already decoded bounded data, generate native NDXR slices and contracts
offline with:

```sh
python3 tools/extract_ndxr_native_slices.py DECODED_ENTRY OUTPUT_DIR
```

The generated files remain external to the product and can be referenced by
the qualified buffer manifest.

Binary NDXR buffers are decoded big-endian without repacking. Primitive-restart
`0xFFFF` markers are retained internally and publish an explicit
`TriangleStripRestart` topology; streams without a marker remain triangle
lists.

The render manifest may additionally reference an optional `camera.tsv`. Each
row is `mission_id` followed by the 16 row-major c218–c221 coefficients. An
optional seventeenth field `column_major` records the retail vector operation
`position.x*c218 + position.y*c219 + position.z*c220 + c221`; this makes the
constant layout explicit instead of silently transposing it. When present, the
renderer applies that qualified homogeneous transform; when absent, it keeps
the deterministic WorldFrame camera path. No camera values are synthesized
into the manifest.

Texture rows may append `source_path` and `source_size` after the existing
FNV-64 field. The loader then verifies the external NTXR slice byte-for-byte
by size and FNV-64 before accepting the drawable; archives remain outside the
runtime package.

Material rows may append a non-zero `mate_id` after `base_color`; it is carried
into the generic shading identity alongside `gidx`.

For a verified offline decode, a `.ppm` source path is also accepted; its P6
pixels are sampled with the decoded NDXR UVs. This is a comparison/debug lane,
not a substitute for the final MATE/GIDX/BC3 retail binding.

The bounded BC3 decode used by the current diagnostic lane is reproducible with:

```sh
python3 scripts/probe_ntxr_bc.py SLICE.ntxr decoded.ppm swap16
```

For a renderer-only Mission 01 developer smoke, combine one F-16 slice and one
terrain slice:

```sh
python3 tools/make_mission01_native_manifest.py MANIFEST_DIR \
  --f16 F16_SLICE --terrain TERRAIN_SLICE [--terrain TERRAIN_SLICE ...] \
  [--extra ASSET:SLICE:KIND:STABLE ...] \
  [--texture STABLE:PPM ...] \
  [--camera QUALIFIED_CAMERA_TSV]
ac6-native --validate-manifest MANIFEST_DIR/manifest.tsv
ac6-native --present-manifest MANIFEST_DIR/manifest.tsv 1
# Optional deterministic native readback before presentation:
ac6-native --present-manifest MANIFEST_DIR/manifest.tsv 1 native-color.ppm
# Optional verification readback: native-color.ppm plus little-endian f32 depth.
ac6-native --present-manifest MANIFEST_DIR/manifest.tsv 1 native-color.ppm native-depth.f32
```

This smoke proves binary geometry loading and native submission only; it is not
the retail parity gate until the oracle reference pack passes comparison.

Camera rows used by the parity comparator must carry the explicit `qualified`
token (and may additionally carry `column_major`). Unqualified camera rows are
accepted only by developer presentation paths and fail the oracle gate.

`--extra` appends another qualified NDXR slice to the catalog, launch, render,
and material contracts. Use it for discovered Mission 01 sky/cloud/map-object
resources (such as `165:SLICE:sky:sky165`); the tool never synthesizes or
packages missing retail bytes.

An optional `controls` manifest row points to the strict SDL profile described
above. When present, the frontend smoke validates the profile before traversing
the frontend; omitted controls retain the compiled default mapping.

`--texture STABLE:PPM` attaches an externally decoded PPM to a drawable stable
ID. The generator records its source, byte size and FNV hash; the native loader
verifies these before sampling. The image remains outside the package.

Verify the durable retail slice inventory before generating a manifest:

```sh
python3 tools/verify_mission01_slice_inventory.py reports/ac6-mission01-retail-slices.tsv
```

Generated manifests also include `input.tsv`; `--frontend-smoke MANIFEST 1`
checks the natural Title → New Game → Briefing → Hangar → Loading → Mission
transition without forcing a mission state.

The SDL adapter can load a qualified external controller profile with
`SdlInputProfile::load_manifest`. The strict TSV format is one `key<TAB>decimal`
per line (comments beginning with `#` are ignored) and requires the four axis
IDs, three inversion flags, and eight distinct keyboard scancodes. Unknown,
duplicate, incomplete, or out-of-range entries are rejected; the profile is
never embedded in the native package.

Checkpoint rows contain `tick`, player position XYZ, pitch/roll/yaw, camera XYZ
and camera-target XYZ, separated by tabs. The comparison runs the replay three
times, rejects nondeterminism, renders the final frame, and applies the fixed
simulation, SSIM, coverage and depth thresholds declared by
`Mission01Thresholds`.

On every completed comparison, `OUTPUT_DIR` receives `native-color.ppm`,
`native-depth.f32`, `color-diff.ppm` and `comparison.json`. Exit status is zero
only when all gates pass. A missing, altered or incomplete reference fails
before simulation. No retail-parity claim is valid without this positive run.

`SaveStore` writes atomically as `AC6SAVE` version 3 and persists the fixed-step
sub-tick accumulator; versions 1 and 2 remain readable with a zero
accumulator. `ReplayLog` uses the versioned `AC6RPLY` input stream, so a save
followed by replay resumes on the same simulation boundary.

Qualified headless validation uses:

```sh
SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build --output-on-failure
```

`cmake --build build --target package` creates a Linux tarball containing the
native binary, headers, README and indexing/sealing tools only; retail archives,
oracle captures and extracted assets are intentionally excluded.

Run `python3 tools/audit_native_package.py build/ac6-native-0.1.0-Linux.tar.gz`
to fail closed on forbidden package entries or binary dependency markers.

## Campaign progression boundary

`include/ac6/campaign_progression.h` defines the renderer-independent campaign
contract. A product manifest supplies exact selector/DPL/DATA.TBL routes;
`CampaignProgression` validates prerequisites, loadout readiness and objective
completion, then emits a deterministic `AC6CAMP` snapshot. Unknown or
unqualified retail routes must remain outside the product manifest.

`MissionExecution` accepts an optional progression boundary. When supplied,
launch requires the mission to be active in that campaign; objective success,
mission completion and objective failure are propagated only after their HSM
preconditions pass. Standalone developer fixtures may omit the boundary.

`CampaignSaveStore` persists completed campaign records separately as bounded
`AC6CSAV` files. It uses a temporary sibling and atomic replacement, and
rejects malformed or duplicate slots without mutating an existing store. The
flight snapshot remains the historical `AC6SAVE` contract until a combined
session format is qualified.

`MissionDebrief` exposes a stable native result view with outcome, objective
counts and radio history for `InProgress`, `Success` and `Failure` states.

`SessionSaveStore` combines a flight snapshot and campaign snapshot in a
bounded `AC6SESS` file. It preserves both legacy stores, validates finite
fixed-step state and sorted campaign records, and uses the same atomic
temporary-sibling replacement boundary.

An optional `campaign` row in the native manifest points to a strict TSV with
`mission_id`, `selector`, `dpl_resource_id`, `data_table_entry`, objective
count, and a comma-separated prerequisite list (`-` for none). The validation
path loads this table atomically; it does not infer missing mission routes.

Audit the 15-mission provenance catalog with:

```sh
python3 tools/audit_campaign_catalog.py reports/ac6-pal-campaign-catalog.json
```

The current PAL evidence is intentionally reported as `1 qualified`, `1
partial`, and `13 unqualified`; unqualified rows cannot enter the native
runtime route.

`MissionExecution::Checkpoint` is the in-memory pause/restart boundary for
flight, HSM, objectives, radio history and combat units. It rejects malformed
states transactionally and refuses checkpoints while a projectile is in
flight. `AC6SESS` version 2 persists the full checkpoint with bounded HSM,
objective, radio and combat-unit state, while version 1 files remain readable.
Version 3 also persists the ordered mission-sequence entries and their
published/pending state; version 4 adds active radio playback state. Versions
1 through 3 remain readable.

`MissionExecution` aborts automatically on player destruction or an optional
failure tick, propagating failure to an active campaign and debrief. The
failure tick remains an explicit generic configuration until retail timing is
qualified.

`FrontendController` can bind `CampaignProgression` to enforce available
mission selection, briefing, Hangar loadout and `Active` state before entering
Mission. Unbound frontend fixtures retain the developer-only state route.

`MissionWaveDirector` publishes deterministic mission waves by tick into both
the unit and combat registries, with atomic duplicate rejection and despawn.
Retail wave parameters still require a qualified mission manifest.

`RadioPlaybackService` resolves audio/subtitle assets, enforces exclusive
playback, tracks completion/interruption and freezes while the mission HSM is
paused. XMA decoding and retail timing remain replaceable service boundaries.

`MissionSequenceDirector` schedules objective and radio events by mission tick
and order, and dispatches them through `MissionExecution` so campaign/HSM
preconditions remain authoritative. Its ordered publication state and active
radio playback are included in `AC6SESS` versions 3 and 4.

`CombatWorld` is the generic combat boundary for active units, faction-aware
target locking, weapons, projectiles, collision and damage. `MissionExecution`
initializes its unit frontier on launch and exposes `lock_target` and
`fire_weapon`; retail weapon parameters remain unqualified until sourced from
the campaign manifest and binary evidence.

Audit the machine-readable code reachability inventory with:

```sh
python3 tools/audit_code_reachability.py reports/ac6-code-reachability-inventory.json
```

The inventory records native covered roots separately from retail unknown
roots. Unknown retail callers/callees require explicit gaps and cannot publish
mission semantics.
