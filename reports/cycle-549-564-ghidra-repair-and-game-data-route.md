# Cycles 549–564 — qualified Ghidra repair and Game Data route

## Result

- Canonical project: `ghidra-projects/ace-combat-6`, PAL XEX SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Headless byte dumps at `0x820F6228..0x820F63C0` and
  `0x8214D360..0x8214D430` match the revision-pinned generated corpus.
- `RepairQualifiedFunctionBoundaries.java` clears the false no-return metadata
  and persistently restores the parser, `SendMsgV`, and aircraft-manager
  dispatcher boundaries. Headless decompilation confirms the parser call and
  the listener-table callback loop.
- The reverted A-only file-selector bridge builds; AC6 CTest passes 7/7 and
  `git diff --check` passes.
- Runtime proves native B/Shift cancels the empty browser (`selector44=3 -> 10`)
  and exposes the native creation prompt (`state40=9`, `type28=5`). B closes
  Game Data; it is not a continue-without-save route.
- The affirmative route reaches FILE 01 and returns to a second creation prompt.
  Cycle 564 shows that this prompt is produced only after the next input edge;
  the recipe now waits for its state before issuing the final YES.

## Validation artefacts

- `cycle-549-repair-qualified-functions.log`
- `cycle-549-sendmsgv-decompile.log`
- `reports/logs/cycle-562-no-save-confirm/step-09-continue-without-save.png`
  (parent report root)
- `reports/logs/cycle-564-create-save-confirm/step-19-post-slot-create-confirm.png`
  (parent report root)

## Next checkpoint

Replay `ac6-first-mission-create-save-route.steps`. Require the final YES to
leave `state40=9/type28=5`, then require `[ac6-first-campaign-level-bridge]`.
Only after that revalidate the loadout with both force options false.
