# Cycle 1508 — the camera rotation core is retail-bounded

## Qualification

- Canonical Ghidra project `ghidra-projects/ace-combat-6`, PAL
  `default.xex`, SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- No Xenia/oracle run and no retail bytes were copied into the native product.
- The function boundaries and scalar constants below were read from the
  canonical import; the generated recompilation listing was used only as a
  control-flow cross-check.

## Producer and call site

`0x82263A50` calls `0x82262A28` on its per-frame campaign camera update path
when `manager+0x3C4 == 0`. The call passes the manager as `r3`, the frame delta
in `f1`, and the camera/player target object in `r5`. The target-selection part
of `0x82262A28` is still upstream work: it reads mode/player state and chooses
the three target values.

The state-writing block is now isolated and typed in
`retail_mode2_camera.h/.cpp`:

```text
0x82262E68..0x82262E8C  shortest-path wrap for +0x3A4 (±2π)
0x82262E90..0x82262EC4  fused interpolation, stores +0x3A4 then +0x3A0
0x82262ED0..0x82262EEC  snap +0x3A0/+0x3A4 to zero below 0.001
0x82262EF4..0x82262F10  fused +0x3A8 interpolation
0x82262F14..0x82262F84  final [-π,π] wrapping for all three fields
```

The constants are the retail words, not host-library substitutes:

```text
0x82005E9C  0.001
0x82069BB0  +π
0x82069BF0  +2π
0x8206A044  -π
```

`step_mode2_camera_rotation()` takes the values after the unresolved
target-selection multiplications, preserves the retail fused multiply/add
grouping and carries a deterministic `RetailMode2RotationState`. The
`+0x3A8` path intentionally does not receive the 0.001 snap; the PAL listing
only snaps `+0x3A0` and `+0x3A4`.

## Controls

`ac6-retail-mode2-camera-tests` now checks:

- float-bit-pinned interpolation of all three fields;
- shortest-path crossing at the ±π seam;
- the two-field 0.001 snap and the unsnapped `+0x3A8` field;
- non-finite state refusal;
- the existing 15-group sealed-cache mode-2 transform coverage.

This is a closed scalar transition, not a JV claim. The session still lacks
the live target selector, player pose producer and state carry-over, so
`camera_runtime_state_retail`, `camera_pose_retail` and `jv_eligible` remain
false.

## Validation

```text
Release build                                        pass
CTest, SDL_AUDIODRIVER=dummy + Xvfb                  67/67
tools/tests                                           87/87
sealed-cache audit                                   15 missions / 17 blobs
campaign matrix SHA-256                              62a8d2199baa0d8811946eace99f3367338c795af30eb034a7871f2a1273ab87
mission01-final-gate-v3 --require JF                pass
mission01-playable-gate-v1 --require JF             pass
contract artefacts                                  pass (146/146)
contract addresses                                  321/321
contract derivations                                52, gaps 0
C++ complexity                                      221 files
class map --require J2                              pass (811 vtables / 1619 rejects)
ac6-retail-mode2-camera-tests (sealed cache)       pass
ac6-retail-mission01-scene-bundle-tests            pass
```

The complete build, CTest, Python corpus and gates remain to be rerun after
this cycle. The next camera boundary is the target-selection block feeding
`f25/f28/f29/f31/f24` in `0x82262A28`, followed by the player locator producer.
