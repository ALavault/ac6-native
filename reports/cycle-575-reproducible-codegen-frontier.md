# Cycle 575 — reproducible codegen frontier

Target: PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Result

- Added an SDK graph regression fixture proving that an exact declared
  function entry takes precedence over containment in another function's
  blocks.
- Corrected conditional-branch and direct-call classification accordingly.
- Removed runtime-qualified false `rex_sub_*` starts that split guest loops.
- Two independent codegen runs produced byte-identical manifests; manifest
  SHA-256:
  `c6c0b2b5cd5b6e05383a55d0da56abcdb5e950a43f2188424d6ff4b5945541c1`.
- The state-driven fresh-profile route reaches campaign configuration without
  loadout force flags.

## Validation

- Focused AC6 and graph checks: 8/8 passed before the latest config-only
  pruning.
- Corrected generated code contains local labels for the former fatal branches
  at `0x82331CD0`, `0x82383EE8` and `0x823616F4`.
- Cycle 575 passed the prior startup fatal and stopped later during campaign
  transition at `0x822EEF90 -> 0x822EEF5C`.
- The revision-pinned literal mapping declares starts at `0x822EEF34`,
  `0x822EEF38`, `0x822EEFBC` and `0x822EEFC0`, not the configured synthetic
  starts `0x822EEF28`, `0x822EEF58`, `0x822EEF80`, `0x822EEFC8`. Those four
  false starts are now removed.
- Cycle 576 regeneration completes successfully: the former fatal emission is
  absent and `0x822EEF5C` is emitted as a local loop label.

## Next checkpoint

Regenerate and rebuild, then rerun the same fresh-profile route. Accept only a
new exact runtime frontier or successful advancement past `START MISSION`; do
not restore force flags and do not edit generated output.

## Residual risk

The configuration still contains many inferred `rex_sub_*` starts. They must
not be removed in bulk: qualify each changed boundary against executed failure,
the revision-pinned literal corpus and canonical Ghidra evidence where needed.
