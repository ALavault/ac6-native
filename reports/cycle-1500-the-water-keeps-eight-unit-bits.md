# Cycle 1500 — the water keeps eight-unit bits

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`; no Ghidra or oracle run
  occurred. Existing Ghidra changes and untracked scripts were preserved.
- Entry 119 came only from the external sealed cache, index
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  No retail bytes or local cache path enter the commit.

## Compact terrain upload source

The store-backed map assets now retain retail's terrain representation instead
of expanding and rebuilding a million quads each frame:

```text
patch selector grid                         16 × 16 bytes
height blocks                               74 × 65 × 65 floats
terrain atlas cells                         256 × 256
distinct (page, tile) bindings              1,390
atlas pages                                 7
page dimensions                         6 × 4096², 1 × 4096×1024
```

The `.mta`/`.mti` join resolves all 65,536 terrain cells to a bounded page and
tile. Every tile fits its page; this catches the seventh page's quarter-height
layout instead of normalising it as 4096². The resource intentionally stops at
`(page, tile)`: the diagnostic tools choose UV orientation and a centred gutter,
and neither choice is promoted into the accepted path.

## Water stays exact

MCA/MCI/MCD are converted once to a host-endian upload form:

```text
coarse selectors                            256
cell-to-block lookups                       4,864
unique 64 × 64 bit blocks                   413
bit resolution                              8 world units
```

This preserves the source's full bit grid. It does not classify a 128-unit
terrain quad from its centre, which would move shore and river boundaries.
The persistent query was compared with `MapWaterGrid` at 65,536 off-lattice
positions, including the negative-coordinate truncation that previously caught
a tool error: 65,536 agreements, zero mismatch.

The asset remains move-only and owns the terrain, atlas and water sources to
which it exposes spans. A Vulkan backend can upload the compact tables once;
no per-frame registry walk, decompression or topology allocation is required.

## Validation

```text
Release build                                      pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb      66/66
tools/tests                                        87/87
terrain atlas cells                            65,536/65,536
off-lattice water differential                 65,536/65,536
sealed-cache audit                                  17/17
mission01-final-gate-v3 --require JF                  pass
mission01-playable-gate-v1 --require JF               pass
contract addresses                                321/321
contract derivations                           52, gaps 0
contract artefacts                                146/146
```

## Residual boundaries

JV is not passed. Atlas UV orientation/gutter, terrain draw composition, camera
group/opening-view selection, active units, sky and tree model resolution still
block an accepted frame. Lighting and fog remain named visual approximations.
The Vulkan upload and sustained timing gate have not run. JP and the persistent
frontend/localisation path remain later work.
