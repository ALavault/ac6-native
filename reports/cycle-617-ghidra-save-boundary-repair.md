# Cycle 617 — SHA-qualified campaign-save boundaries

Date: 2026-08-03

## Qualification

- Project: `ghidra-projects/ace-combat-6`.
- Module: PAL `default.xex`.
- XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Ghidra: 12.1.2, headless, analysis disabled for the repair pass.
- Repair log: `reports/ghidra-cycle-617-repair.log`.
- Repair-script SHA-256: `78b26fe91748feb6187c50d77331fc519edbeb04dfa4d5a69787b3609a0d121b`.

## Boundary evidence

The canonical import previously represented `0x8218C238` and `0x821C37E0`
as two-instruction tail-call functions. The revision-pinned generated corpus
contains executable bodies ending at `0x8218CCA4` and `0x821C3BE0` respectively.
The PAL bytes are now qualified before changing either boundary:

```text
0x8218C238..0x8218CCA4  length=2672  SHA-256=f4f7e8f2382789a084bbf648bfb7884cbc87d7c8242f1efddcd7b01cacf289d4
0x821C37E0..0x821C3BE0  length=1028  SHA-256=4903a839277c318929347d4790840deb82000f5541d1e5bf575f63a973642120
```

The script repairs only those exact ranges and names them
`CampaignSaveOuter_8218C238` and `CampaignSaveDialog_821C37E0`. It reads
individual loader bytes because the imported XEX returns zeroes from
`Memory.getBytes` for undefined instructions; the range guard therefore covers
the actual bytes rather than the current listing state.

The first repair pass exposed one additional importer defect: the three ABI
save helpers were marked no-return and their prologue calls had a
`CALL_RETURN` flow override. The script now qualifies and repairs
`__savegprlr_14` (`0x82382EC0`), `__savegprlr_22` (`0x82382EE0`) and
`__savegprlr_29` (`0x82382EFC`), then clears the overrides at `0x821C37E4` and
`0x8218C23C`. The headless pass completed with `Save succeeded`.

A direct PyGhidra check after the repair reports:

```text
CampaignSaveOuter_8218C238  body=[0x8218C238,0x8218CCA4]  652 instructions
CampaignSaveDialog_821C37E0 body=[0x821C37E0,0x821C3BE0]  225 instructions
```

The decompiler now emits the complete outer registration/state path and the
dialog state machine (states 0–9 and terminal 10/11 paths), instead of the
previous two-instruction tail-call stubs. No generated C++ was edited. The
existing repairs for `0x820F62B0`, `0x820F6330` and `0x8214D390` remain guarded
by the same XEX identity.

The fresh bridge export completed with 8,827 functions. The repaired outer
body confirms the two listener registrations in the table at `0x8293B800`,
followed by manager setup calls through slots `+0x84` and `+0x68`. The dialog
body exposes a concrete state frontier: state 0 calls `0x821CFE18`, state 6
calls `0x821C56F8` and branches on the dialog result at `this+0x24`, while
state 8 calls `0x821C3BE8` before entering terminal state 11 or state 9. These
are static producer/consumer boundaries for the next natural route probe; they
are not a license to synthesize a state or event.

## Scope and next use

This closes a static function-boundary contradiction and makes the campaign
save lifecycle suitable for a fresh bridge export. It does not prove that the
title/video route reaches the save dialog, nor that the lower-left capability
polygon is populated. Dynamic work remains gated on a natural readiness path
with both force options disabled.
