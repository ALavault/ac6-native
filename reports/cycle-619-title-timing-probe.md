# Cycle 619 — title timing reaches Game Data dialog

Date: 2026-08-03

## Qualification

- Binary SHA-256: `36ca874d6eecbcdab4dcd3fdbc60acd06187ecc913082d5e928d70b73cc223c9`.
- Both loadout force options were left false; fresh user-data root:
  `/tmp/ac6-cycle-619-user`.
- Route: Escape at 35 s, space/A at 42 s; no guest write or synthetic event.
- Runtime output: `reports/logs/cycle-619-title-timing-probe/`.

## Result

The corrected timing leaves the sponsor sequence and title screen. At 50 s the
runtime renders the natural `No ACE COMBAT 6 Game Data / Create new Game Data?`
dialog with `NO` selected; the same dialog remains at 70 s and 90 s because
the A press at 42 s preceded dialog creation. Logs show real XAM edges
(`0x0010` at the first press and `0x1000` at the title exit), focus is valid,
and no save/loadout hook is claimed.

This closes the cycle-616 “input during intro” hypothesis and establishes a
state-timed next step: move the dialog highlight to YES, then confirm after the
dialog is visible. It does not yet prove game-data creation or mission entry.

## Artifacts

- 5,991 `PRESENT` lines; clean bounded teardown;
- log SHA-256: `4ca74dfaae91c61d46cee181eb8629b7029a0c4a637684c2bfe79922fdc86926`;
- `t30.png`: `0283f512579bf760368e8e82ce0ed1587070c4db1ff742312ea2515690544232`;
- `t40.png`: `af56d1285648eedc62295c5bb950b6eeb502dd1caa457dccb1c69a5b8975158d`;
- `t50.png`: `3dbd7576e664211ce44adf7c2f8fb08323b819078a70b84a55cac301c1e368ab`;
- `t70.png`: `0fb7c3620fe14aa634301fc042fab5f1bdfff62a3329b11efe6c842484c1b829`;
- `t90.png`: `7fc3d096b1a498b926678ba5dddd8690f693e103281ce5e1db0da05782fdd032`.
