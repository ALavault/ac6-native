# Cycle 1502 — the cell is four fans

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`, opened read-only.
  The listing for `0x820FD768` is complete at 365/365 instructions and the
  listing for `0x820FD418` at 82/82 against `.pdata`.
- Qualified cache index SHA-256:
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  No retail byte enters the commit.
- No oracle, controller session or generated recompiler output was used.

## Retail topology

`0x820FD768` constructs both terrain buffers rather than consuming a model
file:

```text
0x820FD79C-7BC  bases (0,0), (0,2), (2,0), (2,2)
0x820FD7D0-8FC  ten local x/z pairs around each quadrant centre
0x820FD930-DA54 ten sequential u16 indices, then 0xFFFF, four times
                 0x58 bytes per cell, repeated to 0x5800 = 256 cells
```

The draw callback closes the primitive rather than leaving it to the shape of
the data:

```text
0x820FD504  li r4,0x5
0x820FD548  bl 0x821DF2C0
```

The provenance-checked architecture catalog entry `xenia-xenos` maps Xenos
primitive `5` to `TriangleFan`; its local source SHA-256 is
`1224073721a11e332dab47e34555a4be6ca9a896ed842915db8e641f391e48c3`.
This generic mapping interprets the binary-qualified constant; it does not
replace the AC6 listing. The ten vertices are centre plus a closed perimeter,
so each fan produces eight non-degenerate triangles. Four fans cover one 4x4
sample cell with 40 local vertices, 44 restart-bearing indices and 32
triangles.

## Product result

`Mission01TerrainCellTopology` stores that exact local stream and its four fan
indices once. `Mission01TerrainCellInstance` binds it to each of the 65,536
world cells with the exact retail patch-sample base and atlas page/tile.
`resolve_vertex()` computes world X/Y/Z and the shader-qualified UV without an
expanded vertex allocation.

The qualified test checks the literal 40-vertex order, all four restarts, all
32 fan triangles and every instance binding. It resolves all 2,621,440
instance/topology pairs and compares their positions and heights to
`TerrainField::sample`; all match. Invalid instance and topology indices fail
closed.

This deliberately replaces retail's repeated 256-cell index buffer by a
shared topology suitable for native instancing. It changes storage, not the
primitive, winding, sample address, world transform or atlas binding.

## Validation

```text
qualified terrain topology                    65,536/65,536 cells
resolved vertices                         2,621,440/2,621,440
Release build                                               pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb              66/66
tools/tests                                                  87/87
sealed-cache audit                                           17/17
mission01-final-gate-v3 --require JF                          pass
mission01-playable-gate-v1 --require JF                       pass
contract addresses                                          321/321
contract derivations                                     52, gaps 0
C++ complexity                                           216 files
contract artefacts                                          146/146
```

## Residual boundaries

JV is not passed. The topology is a persistent source, not yet an accepted
frame: terrain and exact eight-unit water still need CPU-reference
composition with placed city assets under an explicit retail camera. Active
mission units, sky and vegetation remain outside that frame. Lighting and fog
remain named visual approximations; Vulkan timing, JP, frontend and PAL
localisation remain open.
