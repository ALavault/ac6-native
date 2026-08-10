# AC6 native Linux — developer preview

This package contains only the native Linux runtime boundary and headers. It
does **not** contain retail archives, extracted assets, or an assertion of
frame parity with the PAL disc.

The PAC/DATA archives are import-time inputs only. They are never installed or
opened by the existing Mission 01 comparison lane, which consumes bounded,
content-addressed buffers plus a local reference pack.

## Retail import and sealed cache

```sh
ac6-native import --source DATA_ROOT [--cache CACHE_ROOT]
```

The importer requires the qualified PAL `default.xex`, `DATA.TBL`,
`DATA00.PAC` and `DATA01.PAC` identities. It parses the complete big-endian
table with bounded 64-bit ranges, then descrambles and raw-DEFLATE decodes the
common camera resource at entry 1, campaign entries 9–23 and the qualified
Mission 01 world resource at entry 119. A wrong identity, duplicate request,
truncated range, size-limit violation or decode error fails before a new
generation is published.

The default cache is `$XDG_CACHE_HOME/ac6-native`, or
`$HOME/.cache/ac6-native` when the XDG path is absent. Payloads live under
`blobs/sha256/HH/DIGEST`; a versioned binary index retains source ranges,
storage/payload sizes and both SHA-256 identities. `current` is replaced only
after every blob and the index have been written and synced. Abandoned
`.staging` content is never a published generation.

Audit an imported 15-mission cache against the durable campaign catalog and
dependency inventory with:

```sh
python3 tools/audit_ac6_retail_content_cache.py CACHE_ROOT \
  --matrix-out reports/ac6-pal-campaign-import-matrix.json
```

`RetailMission01SceneBundle` opens world entry 119 directly from that store.
It validates the nested root/map/mapset FHM hierarchy, then opens terrain,
water and all 4,318 map placements through the native readers. Its binding
audit covers 178 NDXR files and resolves every one of their 4,326 material
texture references by GIDX against the 192 NTXR wrappers in the same retail
bundle. The explicit test-payload overload carries no store provenance and is
not an accepted interactive content path.

`RetailMission01MapRenderAssets` turns the placed-city branch into persistent
CPU resources without extracted filenames. It decodes 4,318 NDXR record
primitives once (112,719 vertices and 138,610 strip indices), retains the 170
referenced NTXR wrappers for explicit one-time upload, and builds 4,226 draw
commands after applying retail's 92 skips. Each command is a fail-closed
`(model selector, record index)` binding; the 4,318 source pairs cover the
4,318 records exactly once and their record-name class agrees with the `.pdl`
class in every case. No rotation or substitute transform is invented.

The same asset owner retains the compact 256-byte terrain patch grid, all 74
65×65 height blocks, 65,536 terrain-cell atlas bindings and the seven NTXR
atlas pages. It also decodes MCA/MCI/MCD into 4,864 host-endian lookup entries
and 413 bit blocks, preserving the native eight-world-unit water resolution.
The terrain draw source carries retail's exact four restart-terminated
ten-vertex fans (40 local vertices, 44 indices and 32 triangles) once, then
binds that topology to 65,536 persistent cell instances and their exact height
sample bases. This replaces retail's repeated 256-cell index batches without
changing their topology or expanding 2,621,440 vertices at load time.
The atlas binding includes retail's two terrain-vertex-shader UV transforms:
world X maps to U, world Z maps to V, and the 272-pixel tile is contracted to a
255.5-pixel inner span about its centre (8.25 pixels per edge). Page 7 keeps its
own 4096×1024 vertical step. The common camera table is selected directly by
the loadout's retail aircraft ordinal for all 15 aircraft and three view modes;
the public native ID is that ordinal plus one because zero means unset. The
opening view and final scene composition, rather than camera-group or UV
selection, now remain the accepted-JV boundary.

The matrix contains identities, ranges, hashes, observed formats and remaining
boundaries, but no retail bytes or machine-local cache path. Only Mission 01 is
marked playable-supported.

## Retail session

```sh
ac6-native --retail-session SCENARIO_PAYLOAD 1 OUTPUT_DIR
```

Plays Mission 01 from the retail scenario container and from nothing else:
there is no manifest argument, and none is read. The world's units, factions,
player and four sub-missions all come out of the payload; the session runs 1800
fixed ticks of input, flight, camera and HUD over it, and the mission ends when
its own sub-mission script runs out — the command exits non-zero if it does not.

`OUTPUT_DIR` receives `retail-session-cli.json` and `retail-session-hud.ppm`.
The name is not `retail-session.json`: that one belongs to
`ac6-retail-session-tests`, which writes it only after its assertions pass
and which the JF and JV contracts cite. Two producers under one name meant
the last writer won (cycle 1269).

The payload is retail content and is never shipped with this package. The
session declares no external asset, so the runtime's `mission_ready` flag stays
false: frame parity with the disc is not claimed anywhere on this path.

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

Launch TSV rows accept the legacy `mission_id<TAB>player_entity<TAB>units`
form, or an optional fourth tab-separated weapon list. Each weapon is encoded
as `id:damage:projectile_speed:cooldown:max_range`; duplicate or invalid
definitions are rejected before launch publication.

An optional `campaign` row in the native manifest points to a strict TSV with
`mission_id`, `selector`, `dpl_resource_id`, `data_table_entry`, objective
count, and a comma-separated prerequisite list (`-` for none). The validation
path loads this table atomically; it does not infer missing mission routes.

Audit the 15-mission provenance catalog with:

```sh
python3 tools/audit_campaign_catalog.py reports/ac6-pal-campaign-catalog.json
```

Generate the native campaign TSV only from qualified catalog routes and a
separate gameplay-definition file:

```sh
python3 tools/generate_campaign_manifest.py \
  reports/ac6-pal-campaign-catalog.json \
  reports/ac6-native-gameplay-definitions.json \
  /tmp/ac6-qualified-campaign.tsv
```

The generator refuses partial/unqualified routes and refuses gameplay
definitions without a qualified route; it never infers selector, DPL, or
DATA.TBL identities.

The current PAL catalog reports `1 qualified` and `14 partial` routes. The
selector-to-DPL and low-ID physical DPL-to-`DATA.TBL` contracts are qualified
for all campaign selectors 1–15, yielding physical entries 9–23. Payloads for
missions 1–15 are decoded and hashed; the bounded dependency inventory is
durable, while missions 3–15 still need semantic dependency qualification.
Partial rows cannot enter the native runtime route.

Asset manifests accept the legacy three-column form
`asset_id<TAB>relative_path<TAB>sha256`. An extended row may append
`byte_size<TAB>dependencies`, with `-` for no dependencies. When any extended
row is present, the loader requires every row to declare a non-zero size,
rejects absolute or parent-traversing paths, verifies each file's size and
SHA-256 against the manifest, and validates the dependency DAG. The complete
catalog is published only after all checks pass; a failed load leaves the
previous catalog unchanged.

The `MissionManifestLoader::load_runtime` overload that accepts
`MissionRuntimeServices` publishes optional input, objective, radio and
sequence, wave, AI and campaign services together with the catalog, assets and
launches. The complete bundle is built in temporary state and published only after the last
manifest succeeds; the legacy overload remains available for callers that
only need the core three databases. The native `validate-manifest`,
`frontend-smoke`, `services-smoke` and `present-manifest` commands consume this
bundle directly, so optional services are not silently reloaded or discarded
at their command boundaries.

The optional `sequence` path uses six columns
`mission_id<TAB>tick<TAB>order<TAB>event<TAB>id<TAB>duration`. Event names are
`activate_objective`, `complete_objective`, `fail_objective` and `play_radio`;
the sequence is sorted and published atomically with the other services.

After a gameplay tick, the native execution closes the mission HSM
automatically when at least one objective exists and every required objective
is complete. Player destruction and the configured failure deadline are
evaluated first, so failure remains terminal when both conditions occur on the
same tick. The explicit `Complete` event remains available for qualified
event consumers and is still validated against campaign progression.

The optional `waves` path uses twelve columns:
`mission_id<TAB>spawn_tick<TAB>unit_id<TAB>owner<TAB>asset<TAB>faction<TAB>`
`x<TAB>y<TAB>z<TAB>health<TAB>max_health<TAB>collision_radius`. A row is
published only when its unit and active combat state satisfy the existing
identity and finite-value invariants. The optional `ai` path uses six columns:
`mission_id<TAB>first_tick<TAB>period_ticks<TAB>entity<TAB>target<TAB>weapon_id`.
Both directors reject malformed, duplicate or empty manifests transactionally;
`services-smoke` and `present-manifest` pass the loaded directors directly to
`MissionExecution`, so spawn and fire scheduling share the same runtime state.

`MissionExecution::Checkpoint` is the in-memory pause/restart boundary for
flight, HSM, objectives, radio history, combat units and the sorted
`AssetRecord` identities used by the mission. It rejects malformed states
transactionally and refuses checkpoints while a projectile is in flight.
`AC6SESS` version 2 persists the full checkpoint with bounded HSM, objective,
radio and combat-unit state, while version 1 files remain readable. Version 3
also persists ordered mission-sequence entries, version 4 adds active radio
playback, version 5 adds active campaign/loadout records, and version 6 adds
resource paths and hashes. Version 7 adds declared resource byte sizes and
dependency IDs. Version 8 adds the unit registry and published-wave state
needed for deterministic mission reloads. Versions 1 through 7 remain
readable; resource identities are
compared against the current manifest during restore, and a legacy checkpoint
without the extended contract fails closed against an extended manifest.

`MissionExecution` aborts automatically on player destruction or an optional
failure tick, propagating failure to an active campaign and debrief. The
failure tick remains an explicit generic configuration until retail timing is
qualified.

Flight axes normalize the full signed SDL range to `[-1, 1]`, including the
asymmetric `-32768` endpoint; throttle remains the bounded byte range
`[0, 255]`.

After each gameplay tick, combat activity is synchronized into the unit
registry before `WorldFrame` publication, so destruction and despawn update
the active-unit count used by HUD, rendering and checkpoints.

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

The inventory records native covered roots separately from retail partial and
unknown roots. Qualified retail addresses do not imply a complete call graph;
all partial/unknown callers, callees, routes, and templates require explicit
gaps and cannot publish mission semantics.
