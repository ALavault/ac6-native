# Mission 01 gameplay tick report

Current qualified checkpoint: cycle 782 (`bridge`). See
`cycle-782-canonical-pitch-input.md` and the bounded JSONL files under
`analysis/gameplay/`.

`CModeTaskGame`, the mission-manager update, `UpInput`, `UpObj`, `UpCam`, and
`UpRadio` all continue after the black-world HUD appears. The UnitManager has
230 objects and contains an exact 256-byte `CAce6UnitPlayer` wrapper. Its live
list at `+216/+220` contains one stable child, update `0x822A6710` executes and
the copied transform changes. A pitch-only run with null windows observes a
transform response immediately after `ly=32767`. Direct sampling rejects
`child+380/+382/+536/+538` as that canonical command. Cycle 782 directly joins
XAM left-stick Y to canonical `device+0x3E` at `0x8234D378`; the consumer from
that field to the player child remains open. This is active gameplay infrastructure,
not a blocked central update loop. Stock/observe confirmation and
the complete HSM/demo/operation/radio chain remain open.
