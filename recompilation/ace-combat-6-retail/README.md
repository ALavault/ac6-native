# Ace Combat 6 retail native recompilation

This is the active AC6 retail product tree. It has two explicit profiles:
`rexglue-oracle` materializes a clean ignored working copy of BSD-3-Clause
`AC6_recomp` commit `09144bb092ad871584808aeead69c395edbd5200`; `native`
materializes only the standalone Xenos/Vulkan Gate 1 sources. Generated C++,
XEX files, ISO files and extracted game data stay under ignored `build/`.

The oracle profile is temporary and never releaseable. The native profile owns
the product renderer/runtime migration; `reconstruction/ace-combat-6` remains a
separate historical target and is not merged here.

The NTSC-U/J retail target is the mandatory bootstrap. PAL Europe Rev 1 remains
blocked until the US binary reaches Mission 01 gameplay with the qualified
96-step route. PAL functions and hooks must then be regenerated and qualified
from `ghidra-projects/ace-combat-6`; the demo project is unrelated.

Gate 1 native renderer smoke path (no retail bytes copied into the source
tree):

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/prepare.py \
  --target ntsc-uj --profile native \
  --xex <qualified-us-default.xex> --iso <qualified-us.iso>
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/build.py \
  --target ntsc-uj --profile native
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/validate.py \
  --target ntsc-uj --runtime native
```

This path validates the typed PM4/MMIO/ring boundary and capsule fixture only;
it does not claim gameplay or release readiness. `--runtime native
--require-release` fails closed until the later runtime, campaign and save
gates are qualified.

An oracle capture is converted only from a bounded JSONL event log:

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/capture_xenos_capsule.py \
  --input <read-only-oracle-events.jsonl> --output <capsule.json> \
  --xex-sha256 6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc \
  --iso-sha256 204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c \
  --ghidra-project ghidra-projects/ac6-us --route-sha256 \
  771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043 \
  --ring-dword-count 64
```

The converter rejects guest writes, sockets, raw retail bytes and the first
unsupported PM4 offset.

Final installation check:

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/audit_native_release.py \
  --prefix workspaces/ace-combat-6/recompilation/ace-combat-6-retail/install/ntsc-uj
```

It rejects nested `bin/bin`, generated C++, retail media and oracle/backend
names in the installed native tree.

From the portfolio root, prepare US media with:

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/prepare.py \
  --target ntsc-uj --profile rexglue-oracle \
  --xex <qualified-us-default.xex> \
  --iso <qualified-us.iso>
```

Run the one permitted generation/build under the repository heavy-job cgroup:

```sh
systemd-run --user --scope --unit=ac6-retail-us-build \
  -p MemoryHigh=16G -p MemoryMax=24G -p TasksMax=128 \
  timeout --signal=TERM --kill-after=30s 45m \
  python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/build.py \
  --target ntsc-uj --profile rexglue-oracle
```

Then run `tools/validate.py --target ntsc-uj --runtime rexglue-oracle`. The installed executable accepts
the ISO directly:

```sh
recompilation/ace-combat-6-retail/install/ntsc-uj/bin/ac6recomp <path-to-iso>
```

Static validation also checks the fail-closed 15-mission campaign manifest in
`targets/ntsc-uj-campaign.json`. Its current inventory may pass while reporting
`release_ready=false`; the final release gate is
`tools/validate.py --target ntsc-uj --runtime native --require-release`, which fails until every
mission has qualified static, gameplay, debrief, visual and control evidence,
the final-binary save round-trip passes, and the shared Vulkan visual boundary
is closed. It also requires one `ac6.retail-campaign-run.v1` aggregate proving
the ordered fresh-process Mission 01..15 chain on that same final binary.

The static campaign evidence is layered and US-specific: bounded payload
structure, Scene/TCAM joins, and child-0 scenario round-trips/schema walks are
recorded under `artifacts/retail-us-*-static-20260828/`. These manifests prove
bytes and parser coverage only; they never promote gameplay or objective
semantics. Runtime promotion remains per mission and fail-closed.

The Mission 01 candidate gate is:

```sh
SDL_AUDIODRIVER=dummy \
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/run_gate.py \
  --target ntsc-uj --output workspaces/ace-combat-6/artifacts/retail-us-gate
```

Run it under the same cgroup/wall-clock discipline as the build. It uses the
qualified and identity-sealed 96-step Mission 01 route, a private Xvfb display
and fresh isolated user data. Before creating a session it verifies that the
validated host exposes every guest-owned synchronization marker used by that
route; a stock host without those markers fails at preflight instead of spending
the single runtime session waiting on an impossible predicate. The runner keeps
the bounded gate log below a raised 128 MiB rotation threshold, then records one
compact `RESULT.json` beside the complete log and 27 captures. The v3 receipt
waits for 30 stable frames with cinematic off, world composition active and a
HUD/UI signature present. It rejects a center mean or standard deviation at or
below 0.05, a center with at most 25% non-black pixels, and any control image
changing at most 5,000 pixels. A successful host-driven run is still only a
candidate. Release promotion requires `--mission-id`, an identity-matched
`--input-replay`, and a route that also captures `debrief` and `post-mission`.
`tools/audit_capture.py` then emits automated v3 gameplay, v2 debrief and v2
visual receipts. It requires renderer world invariants, all five control
deltas above 5,000 pixels, strict replay load/finalization, post-mission
progression, a completed save update and clean teardown. It makes no
pixel-parity claim and accepts no human observation flag.

The final campaign is a segmented chain: one fresh process per mission, the
sealed storage output of mission N as the only storage seed of mission N+1,
and no external shader cache for Mission 01. The cache built by Mission 01 is
then carried forward. Create an identity-sealed
`ac6.retail-campaign-run-plan.v1` containing missions 1..15 in order; each
entry names a workspace-relative route and controller replay plus both
SHA-256 identities. Run it under one bounded heavy-job cgroup:

```sh
systemd-run --user --scope --unit=ac6-retail-us-campaign \
  -p MemoryHigh=16G -p MemoryMax=24G -p TasksMax=128 \
  timeout --signal=TERM --kill-after=30s 8h \
  python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/run_campaign.py \
  --target ntsc-uj \
  --plan workspaces/ace-combat-6/artifacts/retail-us-final-campaign/PLAN.json \
  --output workspaces/ace-combat-6/artifacts/retail-us-final-campaign/run
```

The orchestrator stops at the first failed mission and always writes a bounded
aggregate `RESULT.json`. Bind its 15 receipt sets into
`targets/ntsc-uj-campaign.json`, rerun the save round-trip with the identical
binary, close the renderer boundary, then execute
`tools/validate.py --target ntsc-uj --require-release`.

For one bounded RenderDoc diagnostic, supply a route and exactly one of
`--mission-renderdoc-frame cinematic-d5b4` or
`--mission-renderdoc-frame gameplay-hud`. The runner triggers one capture at
the corresponding named route capture and rejects a diagnostic that does not
produce exactly one `.rdc`.

The save bridge is exercised independently of the gameplay receipt. Use
`tools/run_save_experiment.py` with the validated binary and ISO to create an
isolated user-data tree, walk the Game Data dialog through `type28=10`, then
load the same tree in a fresh process:

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-retail/tools/run_save_experiment.py \
  --binary workspaces/ace-combat-6/recompilation/ace-combat-6-retail/install/ntsc-uj/bin/ac6recomp \
  --iso "<qualified-us.iso>" \
  --output workspaces/ace-combat-6/artifacts/retail-us-save-roundtrip
```

The result records the storage manifest, read-only save markers and the
browser/type-6/post-load captures. `--load-only --storage-seed <directory>`
replays only the load half against an explicitly supplied seed. This route
validates save-container persistence and load dispatch; it is not promoted to
the Mission 01 gameplay receipt until a mission checkpoint is visibly present.

The Linux product compiles SDL's dummy audio backend as part of the qualified
headless contract. `validate.py` rejects a build without it. The runner also
uses its output directory as the process working directory because upstream's
stock `log_file` session default may select `ac6recomp.log` relative to the
working directory even when an absolute command-line path was requested.

The oracle product is fixed to Vulkan, 1280x720, scale 1 and the retail 30 FPS cadence.
D3D12, DDS replacement/export, ultrawide, upscaling, full-resolution effects,
HD terrain, asynchronous shaders and FPS unlock are disabled for the gameplay
gates. ReXGlue remains graphics authority only in `rexglue-oracle`; native lane
has no ReXGlue linkage.
