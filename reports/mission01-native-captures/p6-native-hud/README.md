# Native HUD/debrief acceptance — P6

This bundle is produced by the native `ac6-native-hud-acceptance-tests`
executable. It exercises the data path from `MissionExecution`,
`CombatWorld`, `ObjectiveRegistry` and `RadioPlaybackService` into
`NativeHudRenderer`, then renders live, paused, success and failure states.

The fixture is intentionally marked `fixture=true` and
`retail_semantics_qualified=false` in `native-hud-debrief.json`. It proves the
native HUD and terminal-state mechanics; it does not promote synthetic
objective names, waves or factions to retail Mission 01 semantics.

Measured live state:

- reticle, telemetry, weapon, target lock, radar, objective and radio visible;
- target entity `4098`, primary weapon `7`, radio message `15`;
- `2250` HUD writes and `2218` unique color pixels at 640x360;
- a native wave publishes entity `5000`, increasing active units from `3` to
  `4` with no pending entries;
- pause state is rendered without advancing the native scenario;
- success and failure each render an outcome panel with one completed or
  failed objective respectively.

## Provenance

| artifact | SHA-256 |
|---|---|
| `native-hud-debrief.json` | `d1c5607305a410e38df50d9feed433a04b547bbba50dd052a8dd69dca027af8f` |
| `hud-live.png` | `c1059a42b5f0243633505278393fdd414ab65060811b785ede4542164b87fe64` |
| `hud-success.png` | `76dfd3d139e22ba6f1b8170f45c0a08a32277b8446ba9928f02ff28db3d519e4` |
| `hud-failure.png` | `2e70fadd729c5da98ea4392b93064e54960626e2b3853c08b477cc2b925e2aa8` |
