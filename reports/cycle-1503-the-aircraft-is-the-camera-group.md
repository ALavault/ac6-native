# Cycle 1503 — the aircraft is the camera group

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`, opened read-only.
  `0x82276610` is complete at 119/119 instructions against `.pdata`.
  The two selectors are leaf listings ending in `blr`; neither has a `.pdata`
  row, so no completeness claim is made from that table for them.
- Qualified cache index SHA-256:
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  No retail byte enters the commit.
- No oracle, controller session or generated recompiler output was used.

## Direct selector

The per-frame camera update carries one value unchanged across both calls:

```text
0x82276648  li   r4,0
0x82276650  addi r3,settings,0x70
0x82276654  bl   0x82090438       selected aircraft ordinal
0x82276668  or   r4,r3,r3         same value becomes camera group
0x82276688  li   r5,1             view 1 (branches use 2 or 3)
0x8227668C  bl   0x8225C4A0
```

`0x82090438` reads the active profile, applies the retail mode/slot remap and
returns the word at `settings + 0xAD3C + profile*0xA7E0 + slot*12`. The same
word was already qualified at cycle 1206 as `p` in the player-aircraft DPL
formula `0x15A + 4*p`. It is therefore the zero-based retail aircraft ordinal,
not a model, camera or UI identifier inferred by proximity.

`0x8225C4A0` maps view modes 1/2/3 to indices 0/1/2, computes
`144 * (3*group + view_index)` and returns that record from the 45-record
runtime array. There is no aircraft-to-camera lookup table or heuristic
between these functions.

## Product result

`RetailCameraTable::group_for_loadout()` now performs the direct binding. The
native public loadout stores `ordinal + 1` because zero is already its
persistent unset sentinel; this normalisation is documented on
`CampaignLoadout`. Invalid loadouts, aircraft IDs outside 1–15 and view modes
outside 1–3 fail closed. `record_for_loadout()` is the store-facing API used by
the qualified cache test.

The synthetic test covers all 45 aircraft/view combinations and the four
failure classes. The qualified branch opens retail `DATA.TBL[1]` child 36 and
checks that native aircraft 1 selects group 0's exact `(0,3,15)` offset and
38-degree FOV, while native aircraft 15 selects group 14.

## Validation

```text
direct selector combinations                                      45/45
qualified PAL camera table                                         pass
qualified PAL cache / Mission 01 session                            pass
Release build                                                       pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb                     66/66
tools/tests                                                        87/87
sealed-cache audit                                                  17/17
mission01-final-gate-v3 --require JF                                 pass
mission01-playable-gate-v1 --require JF                              pass
contract addresses                                                 321/321
contract derivations                                            52, gaps 0
C++ complexity                                                  186 files
contract artefacts                                                146/146
```

## Residual boundaries

The camera group is closed, but the opening view remains unresolved. Mode 2's
position path is derived; modes 1 and 3 and the exact opening selection are not
yet product claims. JV is still open on marker-free CPU/GPU composition,
active units, sky and vegetation. Vulkan timing, JP, frontend and PAL
localisation remain open.
