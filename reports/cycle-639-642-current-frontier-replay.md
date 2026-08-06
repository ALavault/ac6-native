# Cycles 639--642 — current first-mission replay boundary

Date: 2026-08-03

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Lane: `bridge`; `ac6_force_loadout_ready=false` and
`ac6_force_loadout_launch=false`.

## Result

Cycle 638 already proves that the campaign resource call at `0x8218F3A0`
returns level 1 and constructs the first-mission task. The historical
`[ac6-first-campaign-level-bridge]` and `live-single-member-wrapper` markers
are therefore obsolete synchronization predicates when level 1 is selected
naturally; their absence is not a PAC/DPL failure.

The native-readiness recipe now waits for `[ac6-first-mission-task]` before
skipping the campaign introduction. That task marker can be enabled by the
focused loadout trace alone. It no longer requires the high-volume UI
dispatcher trace, which measurably slows the variable-length introduction and
invalidates wall-time key schedules.

The bounded replays also exposed a second prerequisite: the shared local user
root may start in the Game Data browser rather than the fresh-profile
`type28=30` prompt. Cycles 639--642 are therefore harness qualification, not
new readiness evidence. The accepted downstream frontier remains the rendered
`MISSION 01 / STANDBY` state from cycles 604/610, with native readiness/status
and capability publication unresolved.

## Validation

- `linux-bridge-relwithdebinfo` rebuild: pass;
- corrected recipe:
  `scripts/ac6-first-mission-loadout-native-ready-probe.steps`;
- no generated C++ edited;
- no force option enabled and no guest field synthesized;
- cycle 642 reaches the natural Game Data selector and records response/type
  epochs, but is excluded from loadout evidence.

## Next exact checkpoint

Run the corrected recipe from an isolated, manifest-recorded user-data root.
At the first three loadout A edges, require the existing
`[ac6-loadout-predicate]`, `[ac6-loadout-state]` and
`[ac6-loadout-selection-transition]` records. The first missing producer of
`manager+35994` (ready), `manager+35995` (status), or the selected aircraft
record is the next implementation boundary. Do not restore either force flag.
