# Cycle 778 — gameplay loop and player census at the black Mission 01 frame

Date: 2026-08-04

## Result

The bounded native Linux run reaches Mission 01's black-world flight HUD in
the declared `bridge` lane. At that boundary:

- `CModeTaskGame` is active: object `0xB8EA0400`, exact qualified vtable
  `0x82064384`, sampled through function `0x8219A140` from frame 10609 to
  frame 11808;
- the X360 mission manager update at `0x8226D1C8` continues through frame
  11809; its primary/secondary vtables are `0x8206457C`/`0x82064648`;
- `UpInput`, `UpObj`, `UpCam`, and `UpRadio` all execute; the UnitManager
  (`0x82055190`) keeps 230 objects;
- the registry contains one exact `CAce6UnitPlayer`, object `0xB2470000`,
  vtable `0x820568D4`, and no `CAce6UnitOtherPlayer`;
- raw input reaches XAM: pitch `ly=32767`, roll `lx=32767`, button `0x0100`,
  and right trigger `255`, each followed by a null control;
- the flight hooks `0x82329B40` and `0x823046A0` execute zero times. A
  follow-up canonical factory check proves that `CAce6UnitPlayer` is a
  256-byte wrapper, so applying the older aircraft-model offset `+10672` to it
  was invalid. The `0xFEFEFEFE` words reject that proposed owner join; they do
  not prove failed player initialization. G8 remains open.

This excludes “no gameplay loop” and “no player object” for this bridge run.
It localizes the next boundary to the player's live child list at `+216` and
the child update/transform path, not directly to the older flight hooks. It
does not prove stock campaign/scenario correctness.

## Positive and negative controls

- Positive: the same wrapper identifies `CModeTaskGame` by exact runtime
  vtable and observes 2,400 calls; each named Up phase also has executed
  samples.
- Positive: the UnitManager census finds the RTTI-qualified player vtable in
  its live 230-entry array.
- Positive: `xam-input` records both nonzero and zero values for each injected
  control.
- Negative: no `ac6-gameplay-flight-input` or
  `ac6-gameplay-flight-force` event exists. This rejects those hooks as the
  currently observed player's direct owner path; it is not yet a physics-fault
  classification.

The visible world remains black with green HUD. Screenshot differences are
63,910 to 67,511 pixels relative to the baseline, but they are HUD/presentation
changes and are not accepted as aircraft motion.

## Identity and validation

- lane: `bridge`; boot-declared interventions:
  `save-dialog-synthesis,force-cvars,fallback-allocator`;
- run: 2026-08-04 11:40:28–11:45:18 Europe/Paris, owned Xvfb `:97`, bounded
  to 285 seconds plus cleanup;
- timing/config: `performance=false`, `unlock_fps=false`, render capture off,
  scale 1x, direct host resolve off, native 2x MSAA on, deswizzle fix on;
- runtime commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, dirty diff SHA-256
  `fe46948412b4160bfcfe3afe58d38d91aa825560eea22b13a6c3b0bdab71f9da`;
- executable SHA-256
  `c5a55554cb9a78ffbd79725043907c7e93470bf4ce28707173fa6fdb7f9d96d5`;
- workspace commit `442c6dbcd5188fb84b056293a3ce7a000bd20669`, dirty diff SHA-256
  `cf1f98a4a8dc32d339353f5c2d0406f275f002451c0b07d82eea40a8acf959b8`;
- canonical XEX/DATA/PAC/generated-corpus identities remain those in
  `analysis/contracts/runtime_contract.json`;
- Vulkan: NVIDIA RTX PRO 4000 Blackwell, driver 595.84, API 1.4.329;
- PAL reconstruction CTest: 63/63 passed, four expected skips;
- build succeeded with `AC6_ALLOW_CODEGEN=OFF`; generated output untouched;
- log SHA-256
  `ffa67fdbb8c91c6835488f6dddf092e8974890c1eafe35571286bf9c06b3882c`;
- no Xenia launched; foreign Pharaoh and Ollama processes were inventoried and
  untouched.

Structured evidence is in `analysis/gameplay/*.jsonl` and
`analysis/mission1/unit_registry_snapshots.jsonl`.

## Next discriminant

Starting from the exact player `0xB2470000`, follow its valid 256-byte layout:
slot `+0x3C` is `0x822A6710`, child-list pointer is at `+216`, child count at
`+220`, and the update copies child transform data into player `+144..+207`.
The success condition is one natural input edge joined through that child to
an orientation/speed/position change. Do not reuse `+10672` on this wrapper and
do not synthesize a controller.
