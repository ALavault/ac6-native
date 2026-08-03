# AC6 native handoff — cycle 626

Updated: 2026-08-03

## Published frontier

- Target: PAL Xbox 360 `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`.
- Force-loadout flags remain disabled; generated output was not edited.
- The state-driven new-profile route reaches Game Data creation, then
  `type28=37 -> 35`, selector state 3 and file-create type 6. Both a fresh and
  existing profile subsequently reach the same type-9/type-5 loop; Mission 1
  loadout/HUD entry is not yet proven.
- Cycle 626 byte-qualified and repaired the save helpers at `0x821C3BE8`,
  `0x821C4FA0`, `0x821C5258` and `0x821C56F8`. The selector reads selection
  from `screen+0x10` and response from `screen+0x0c` in separate epochs. The
  asynchronous create result can legitimately select type 9.

## Local validation

- Last fully qualified baseline before the final logging addition: focused AC6
  CTest 8/8 and bounded runtime smoke without fatal/assert/unresolved markers.
- The final `[ac6-save-inner]` bounded snapshot logging change compiled its
  source translation unit during the interrupted build, but the overall build
  and focused tests were not completed afterward. Treat it as pending
  validation, not as runtime evidence.
- No CI work was performed.

## Next checkpoint

1. Finish the local build and focused tests.
2. Replay the state-gated profile route with verbose UI dispatch logging.
3. Compare `[ac6-save-inner]` state/selector/response/result transitions around
   `0x821C56F8` to distinguish an invalid profile/save-task result from an input
   epoch error.
4. Only after that boundary is closed, resume the Mission 1 loadout/HUD and
   aircraft capability-polygon investigation.

Canonical orchestration handoff:
`/fastdata/lavaulta/auto-re-agent/reports/handoff/CURRENT.json`.
Detailed plan:
`/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/CURRENT_PLAN.md`.
