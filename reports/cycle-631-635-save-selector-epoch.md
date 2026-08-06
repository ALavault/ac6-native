# Cycles 631--635 — save-selector edge epoch

Date: 2026-08-03

Qualification: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
canonical project `ghidra-projects/ace-combat-6`.

## Result

Cycle 631 proved that one edge (`serial=27`) drove both selector calls:
`3->4/type 35->6`, then 17 ms later `4->10/type 6->9`. Cycle 632 watched
`screen+12` (`0xA3317DEC`) and qualified the intervening response writer as an
indirect input-dispatch callback with contextual LR `0x820F6460`. The frozen
corpus does not contain literal-PC instrumentation, so its recorded PC is zero
and LR is not presented as an instruction address.

`FileSelectorEdgeEpoch` now clears a response only when screen, selector state
4 and edge serial match the transition that opened that state. It does not
synthesize an input. Cycle 633 proved the observe lane remains parked at type
6 instead of accepting the stale edge. Cycle 634 proved the bridge lane accepts
a fresh edge and traverses types 6, 8 and 10 before reaching the rendered
campaign introduction. Both loadout force cvars were false.

Cycle 635 repeated the corrected save route but its bounded run expired while
waiting for the campaign-resource marker. It is not readiness evidence. The
separate qualified frontier remains cycle 604/610: rendered `MISSION 01 /
STANDBY` is reachable without readiness/launch forcing, while native readiness
and capability publication remain unresolved.

## Validation

- epoch helper test executable: pass;
- binary-contract unit tests: 2/2 pass;
- frozen corpus: 54 C/C++ files, qualified tree
  `f42fa2c4c1ec3bfb061003ef7074f73881e968ef2719f7f78e59190d1c5af73d`;
- observe, bridge and store-instrumented builds: pass;
- observe and bridge second builds: no work to do (the observe dependency log
  was first quarantined after Ninja reported a truncated generated record);
- `git diff --check`: pass;
- cycle 632 log SHA-256:
  `442647811faa4c6a717a3540aaa081b59e2248eb2aabf54bb3f86760f6045091`.

Artifacts: `reports/logs/cycle-631-file-selector-call/` through
`reports/logs/cycle-635-bridge-native-ready/`.
