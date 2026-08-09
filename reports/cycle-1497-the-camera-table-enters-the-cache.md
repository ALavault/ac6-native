# Cycle 1497 — the camera table enters the cache

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`; it was not opened or
  modified in this cycle. Existing Ghidra state and untracked scripts were
  preserved.
- Retail archives and the generated cache remained outside the committed
  package. No oracle or controller session was used.

## Sealed Mission 01 resources

The default native import now closes three explicit classes of input: common
camera entry 1, campaign entries 9–23 and Mission 01 world entry 119. The real
PAL import published one atomic generation with 17 content-addressed blobs,
631,481,632 decoded bytes and index SHA-256
`349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.

The independent cache audit requires that exact entry set and cross-checks the
qualified ranges, sizes and stored/payload hashes of entries 1 and 119. Two
successive matrix generations were identical:
`62a8d2199baa0d8811946eace99f3367338c795af30eb034a7871f2a1273ab87`.
The durable matrix continues to expose exactly 15 campaign mission rows; the
two common/world records are prerequisites, not extra missions.

## Native camera reader

`RetailCameraTable` opens only `DATA.TBL[1]`, root FHM child 36. It requires the
qualified 55-child root, exactly 6,480 bytes and 45 finite 144-byte big-endian
records. Lookup requires an explicit group in 0–14 and view mode in 1–3;
unknown indices fail instead of selecting a default. The reader exposes the
four offset vectors and the derived `+0x58`, `+0x68` and `+0x6C` fields while
leaving all fields without established semantics unnamed.

This layout follows the PAL loader at `0x821D5EF8`, setters `0x8225C450` and
`0x8225C478`, record selector `0x8225C4A0`, manager copy `0x8225C510` and the
mode-2 consumer at `0x82260930`. The qualified cache control verifies:

- entry 1 root: 55 children;
- child 35: 5,184 bytes = 27 × 192;
- child 36: 6,480 bytes = 45 × 144;
- first view offset `(0, 3, 15, 0)`, ease `7.0`, FOV 38° and alternate 46°.

The aircraft/loadout-to-camera-group selector remains deliberately unresolved;
this cycle does not guess it or connect a camera transform to the accepted JV
frame.

## Validation

```text
Release build                                     pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb     63/63
tools/tests                                       87/87
camera parser positive and negative controls      pass
qualified PAL import and independent cache audit  pass, 17/17
matrix deterministic regeneration                 pass
qualified cache, 15 bundles + camera + session    pass, 230 units
mission01-final-gate-v3 --require JF               pass
mission01-playable-gate-v1 --require JF            pass
contract addresses                                pass, 321/321
contract derivations                              pass, 52/0
contract artefacts                                pass, 146/146 match HEAD
```

## Residual boundaries

JV and JP are not passed. The next bounded step is a store-backed Mission 01
scene bundle over entry 119: nested FHM traversal, terrain/water/placement,
mapset, NDXR/MATE/NTXR bindings, sky and vegetation. Camera group selection,
the exact opening view and the runtime transform remain required before a JV
camera claim. Vulkan persistence, progression, frontend/localisation and the
single human controller session remain later boundaries.
