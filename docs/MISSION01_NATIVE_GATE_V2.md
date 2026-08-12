# Mission 01 native and retail gates

`analysis/contracts/mission01-native-gate-v2.json` separates the mechanisms
implemented by the native runtime from identities qualified in retail data:

- native: combat mechanics, units/waves mechanism, objective flow, essential
  HUD, success/failure debrief, and pause/save/restart;
- retail: unit/wave identity, objective identity and transitions, and the
  retail radio/scenario boundary.

The v2 audit verifies every evidence path and SHA-256 against the artifact
root. Native tests and captures may close native domains only. A passed domain
marked `retail_semantics_qualified: false` cannot close a retail domain.
Bridge, vtable-only, FHM co-location, and fixture evidence are supporting or
native evidence; they cannot be promoted to retail ownership.

The Mission 01 retail gate requires J0, all native domains, both retail
unit/objective domains, qualified retail radio/scenario evidence, and native
pause/save/restart. The current contract reports J0 and J1 passed.

Validate with:

```sh
python3 tools/audit_ac6_mission01_native_gate.py \
  analysis/contracts/mission01-native-gate-v2.json --artifact-root .
```
