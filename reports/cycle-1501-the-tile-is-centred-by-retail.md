# Cycle 1501 — the tile is centred by retail

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`. No Ghidra write, oracle,
  controller or generated-recompiler-name inference was used.
- DATA.TBL SHA-256:
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
  Entry 163 is the bounded DATA00.PAC range `0x7A798000 + 0xF43A1`, stored
  SHA-256 `a75523b8c3beec1ebc3b0ce0292c9816a1218068c29f7e7bc5465e571792dce0`.
  Its decoded 7,836,352-byte FHM has SHA-256
  `5025480f9ed4fc157b7c679b340d7b7069705385d0e0688982275aff6228dea4`.
  No retail byte enters the commit.

## CPU consumer

The flat image closes the values sent to the terrain vertex shader:

```text
0x820FAE34/38  c64 atlas U/V step base = 272/4096
0x820FAE50     owner+0x6D80 = float bits 0x3F707878
0x820FAF10-64  last-page V step = 272 / actual NTXR height
0x820FDB00-CC  per-cell local X/Z and page/tile U/V origins
0x820FD660-734 c64 = origins/steps; c65.x = 0x3F707878
```

Thus page 6 is not normalised to a square: its V step is `272/1024`, while
all U steps and pages 0–5 V steps are `272/4096`.

## Exact shader pair

Entry 163 child 4 is the 64,384-byte NSXR with SHA-256
`054c3ec4e4226d36ce2a7cd95cb6fe42a83ddbea629363850d75f00bf52b1871`.
It contains the two contexts selected at `0x820FD1D0`:

| context | vertex description | bounded container SHA-256 |
|---|---|---|
| `0x04100113` | `vsMapCstCT_Ocean.updb` | `40ad5413839390455c11f2363cf3b119c5ce5c653cf5c40075c4320b4d0571f8` |
| `0x04100114` | `vsMapCstCT_Ocean_Submap.updb` | `6398034eae1086867ca38adbcaa6f3ff0ff3d04767b1692e98a5a198af95b898` |

XenosRecomp revision `990d03b28a27b50277ee5d8d942e1c5f873869d1` translated those exact
containers. Both variants independently reduce their atlas output to:

```text
inner_x = (local_x - 0.5) * 0x3F707878 + 0.5
inner_z = (local_z - 0.5) * 0x3F707878 + 0.5
u = tile_column * u_step + inner_x * u_step
v = tile_row    * v_step + inner_z * v_step
```

The local vertex pair also drives world X/Z in the same order. This proves
`X -> U`, `Z -> V`; it is not an orientation inferred from a plausible image.
The scale times 272 is 255.499992 pixels (float32), leaving 8.250004 pixels on
each side. The diagnostic renderer's centred inset was therefore correct, but
it is admitted only now that the retail consumer establishes it.

## Product result

`Mission01TerrainAtlasUvTransform` retains the page, tile origin, actual-page
steps and exact scale bits. `map_local_fraction()` evaluates the retail order
of operations. All 65,536 cells now produce a bounded transform; the test checks
all of them for exact scale bits, centred 8.25-pixel gutters, `X -> U`, `Z -> V`
and the quarter-height page step.

## Validation

```text
qualified retail map asset test                 pass, 65,536/65,536 UVs
Release build                                                        pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb                     66/66
tools/tests                                                         87/87
sealed-cache audit                                                   17/17
mission01-final-gate-v3 --require JF                                  pass
mission01-playable-gate-v1 --require JF                               pass
contract addresses                                                  321/321
contract derivations                                             52, gaps 0
C++ complexity                                                   186 files
contract artefacts                                                  146/146
```

## Residual boundaries

JV is not passed. Terrain topology/draw composition, an explicit accepted
camera, active mission units, sky and vegetation still have to enter one
audited CPU/GPU frame. Lighting and fog remain named visual approximations.
The Vulkan timing gate, JP, frontend and PAL localisation remain open.
