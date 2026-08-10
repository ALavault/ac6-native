# Cycle 1505 — the mode-2 camera has a retail base

## Qualification

- Target ID `ace-combat-6-pal`; module `default.xex`, Xbox 360 PAL,
  SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project `ghidra-projects/ace-combat-6`, opened read-only.
  Raw words were read over `.text` `0x82260930..0x82260B98`. The `.pdata`
  function declares 153 instructions; the canonical listing recovers 134 and
  is explicitly `TRUNCATED` because 19 VMX128 words are not decoded.
- The missing literal instructions were cross-checked against the generated
  listing in the revision-pinned `AC6_recomp` checkout at
  `dcd41b7457fcac8242f8ef40de83d1719390d5af`. Its configured asset resolves to
  the same qualified XEX. Generated C++ supplied instruction/control-flow
  cross-match only; none was copied, and the checkout's pre-existing config
  modification was not touched.
- Qualified store index SHA-256:
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  No retail byte enters the commit.

## Closed transform

The mode-2 path now has one typed product transform rather than an external
look-at camera:

```text
0x82260948  manager local-offset address = manager+0x30
0x82260960  load selected record stage 0
0x82260964  store it at manager+0x30
0x82260A84  call 0x8225D9F0              DYNAMIC ADJUSTMENT, STILL OPEN
0x82260A88  read player basis at +0x10/+0x20/+0x30
0x82260A98  destination locator = manager+0x140
0x82260AA0  copy the three basis rows to locator+0x10/+0x20/+0x30
0x82260AB0  copy player position +0x50 to locator+0x40
0x82260B00  three vmsum3fp128: transposed basis × local xyz
0x82260B1C  add the transformed xyz to the player position
0x82260B70  rotate locator by manager+0x3A4 through 0x820A9B30
0x82260B7C  rotate locator by manager+0x3A0 through 0x820A99F8
```

`RetailMode2CameraState` carries only those live inputs;
`RetailMode2CameraLocator` carries only the qualified basis and translation.
`resolve_mode2_base_camera_locator()` copies record stage zero and
`transform_mode2_camera_locator()` performs the observed column dot products,
translation and rotation order. Non-finite input and arithmetic overflow fail
closed. The port makes no bit-identity claim for Xenon's `vmsum3fp128`
rounding.

The dedicated test checks identity placement, an asymmetric basis that catches
row/column transposition, ignored W, exact rotation order, record binding and
three refusal classes. Against the sealed store, all 15 aircraft groups resolve
their mode-2 record; group zero pins the base offset
`(0, 0.860000014, -5.9000001, 0)`.

## Product composition

`RetailMission01CpuCompositor` accepts the typed state only with view mode 2.
It validates but does not normalize or rebuild the resulting retail basis, and
uses its three rows directly for world-to-view projection. The qualified
capture therefore no longer consumes the request's external look-at pose. Its
report schema is now `ac6.mission01-cpu-frame.v2` and records:

- `camera_source=retail_mode2_base`;
- `camera_mode2_base_transform_retail=true`;
- `camera_dynamic_offset_retail=false`;
- `camera_runtime_state_retail=false`;
- `camera_mode_selection_retail=false` and `camera_pose_retail=false`.

The last four false fields are deliberate. The capture supplies a bounded
identity player basis/position to exercise the retail transform; it is not yet
the live mission player's locator. No opening-view choice or dynamic
`0x8225D9F0` result is invented, so `jv_eligible=false` remains mandatory.

## Reproducible capture

Two frames through the persistent store-backed compositor agree bit for bit:

```text
terrain considered / visible / rasterised       65,536 / 1,817 / 437
terrain candidate / written triangles           58,144 / 3,920
city considered / visible / rasterised            4,226 / 2,724 / 430
city candidate / written triangles              38,089 / 721
terrain / water / city fragment writes     27,572 / 108 / 761
depth and colour coverage                             27,746
decoded terrain atlas pages                           0–5 (6)
decoded placed-map GIDX textures                          136
marker writes                                               0
colour digest                              c5366eda993a572d
depth digest                               4ef0a2fbe98353f3
```

The full cold product test opens the store, assembles all persistent assets,
decodes textures and renders twice in 8.68 seconds with 770,540 KiB maximum
RSS. This is a reference-path measurement, not the Vulkan 720p30 gate.

## Validation

```text
Release build                                                        pass
qualified mode-2 table coverage                                      15/15
qualified CPU frame and deterministic second frame                    pass
qualified PAL cache / Mission 01 session                              pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb                       67/67
tools/tests                                                         87/87
sealed-cache audit                                                    17/17
mission01-final-gate-v3 --require JF                                   pass
mission01-playable-gate-v1 --require JF                                pass
contract addresses                                                   321/321
contract derivations                                              52, gaps 0
C++ complexity                                                    221 files
contract artefacts                                       146/146 match HEAD
```

## Residual boundaries

JV and JP are not passed. The next camera closure is the bounded semantic port
of `0x8225D9F0` plus the live manager/player fields that select and drive the
opening pose. The capture still has an unresolved black sky and no vegetation,
active mission units or HUD; Vulkan timing, frontend/PAL localisation,
mission-rule progression and the human controller replay remain open.
