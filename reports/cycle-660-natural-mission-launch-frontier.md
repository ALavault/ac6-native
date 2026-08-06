# Cycle 660 — natural Mission 1 launch frontier

Date: 2026-08-03

## Result

The fresh-profile native route now reaches and leaves `MISSION 01 / STANDBY`
without a forced guest write or a loadout/readiness override.

Cycle 658 proves the complete natural menu and loadout path:

```text
save 30 -> 37 -> 35 -> selector44=3
new game data -> campaign -> Mission 01
F-16C -> XMA4 -> STANDBY
```

The aircraft capability polygon, aircraft description, weapon inventory and
XMA4 statistics are populated. This invalidates the former hypothesis that an
unpublished aircraft-stat contract still blocks `STANDBY`.

Cycle 660 adds the missing `A` at `STANDBY`. The next screen is the mission
objective map. A further `A` enters the engine-rendered runway cinematic and
then the airborne launch sequence. The run remains alive until its bounded
425-second timeout. No HUD is visible in the last scheduled capture, so flight
controls are not yet qualified.

## Asset evidence

The 55 runtime-loaded decoded PAC entries contain 57 NUL-terminated
`Mddd_name` symbols (56 unique). IDs 102 and 488 are present; IDs 200 through
208 are absent. The exact loadout event 22 therefore cannot be recovered by a
literal `M200...M208` symbol lookup in this corpus.

Recursive FHM extraction yields 3,176 leaves and nine SWG blobs:

```text
saveLoad, pauseMenu, messageDlg, brandLogoEU, Title, main,
missionTitle, briefing_ms01, loadDemo_common
```

The loadout SWG is not among those 55 persistent runtime captures. The message
scanner and corrected SWG glob preserve this boundary as a reproducible
contract rather than a filename inference.

## Changes

- Native tree: `tools/scan_ac6_message_symbols.py` records exact symbol ids,
  paths and offsets and supports required/forbidden-id contracts.
- Native tree: `tools/parse_ac6_swg.py` now accepts the actual
  `*_SWG_00.bin` names emitted by the FHM extractor.
- Native tree: `tests/test_ac6_asset_tools.py` covers both contracts.
- Research tree: `scripts/ac6-first-mission-fresh-loadout.steps` implements the
  fresh-save route through mission launch and bounded post-launch captures.
- Research tree: stale save predicates in `scripts/ac6-first-mission.steps`
  now use the fields actually emitted by the save-state trace.

## Validation

```text
python3 -m unittest tests.test_ac6_asset_tools
Ran 6 tests — OK

message-symbol corpus contract:
files_scanned=55, matches=57, unique=56
required 102/488 present; forbidden 200..208 absent

cycle 660:
fresh save route passed
campaign transition state=1->2 passed
STANDBY confirmation passed
objective confirmation passed
3D runway/airborne cinematic rendered
bounded timeout only; no runtime fatal before harness termination
```

## Exact next boundary

Replace the fixed 60-second post-objective delay with a read-only visual or
guest-state predicate for the first frame containing the flight HUD. Keep the
cinematic free of control probes. Once the predicate fires, capture a baseline
and apply pitch, roll, yaw and throttle separately, requiring both a qualified
guest-state delta and a non-identical frame. Only then begin the deterministic
mission-completion and save/unlock trace.

Residual rendering defect: aircraft materials/exposure in the runway cinematic
are incorrect (large white/clipped surfaces). It does not prevent the mission
state machine from advancing, but must remain a renderer divergence boundary.
