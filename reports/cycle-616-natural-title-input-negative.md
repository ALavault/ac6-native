# Cycle 616 — natural title/input route negative

Date: 2026-08-03

## Qualification

- Native binary SHA-256: `8987d3497f439aa67174f82f3774f4f16c6a841ea9ecd125733b1a0d4f6911ad`.
- Runtime output: `reports/logs/cycle-616-natural-state-probe/`.
- Both readiness and launch force options were left at their defaults (`false`).
- The run used the existing state-probe steps and bounded Escape/space pulses;
  no guest state or save data was modified by the probe.

## Result

The owned runtime reached the Namco Bandai intro/title video and remained there
for 225 seconds. The harness captured the intro at `t30.png` and a later
unchanged frame at `t225.png`, then the probe was stopped. It never reached
`selector44=3`, the Game Data route, or the loadout/save hooks.

Input delivery is real but not sufficient to establish a transition:

- `ac6-confirm` recorded an Escape/confirm edge (`0x1000`);
- the keyboard probe reported `active=true` and `has_focus=true`;
- the XAM path observed the corresponding guest button state;
- no `ac6-save-outer`, `ac6-campaign-resource`, selector, or loadout dispatch
  record appeared.

This is a negative route/harness checkpoint, not evidence for a mission or HUD
transition. The next dynamic attempt must first qualify the title/video exit
predicate or use a previously verified profile state; repeating the same
timing would not add evidence.

## Artifacts

- log SHA-256: `aca122e4a6a8e4370af23bc40ef32932708e6028058ab9288b6fe1b643d80327`;
- title capture SHA-256: `ba3d70a91cf7277356d511e2afa638a5ffe37fca092bc18c87aa4212977f5015`;
- unchanged late capture SHA-256: `93606f15d87a822f91337c506a52c643f1d954574d97a11238a21372e75f02f5`.
