# Cycle 1499 — the low half is the record

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`; no Ghidra or oracle run
  occurred. Existing Ghidra changes and untracked scripts were preserved.
- Retail entry 119 came only from the external sealed cache, index
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  No retail byte or machine-local cache path enters the commit.

## The placement record closes

The `.pdl` tag's nine-bit field was already established as the `parts/%d`
model selector. Its low sixteen bits were only named an identifier. Joining
them against the bounded NDXR record arrays closes the remaining ambiguity:

```text
placement instances                         4,318
(selector, low16) pairs in range             4,318
unique pairs                                 4,318
NDXR records across the 170 models           4,318
selected-record class matches tag class      4,318
accepted / retail-skipped                 4,226 / 92
```

The pairs exhaust the NDXR population exactly once. The low field is therefore
the record ordinal inside the model chosen by the selector, not another model
identifier. The native placement test enforces the per-record join, not only
the earlier four global histograms. `MapInstance::record_index` now carries the
established meaning and the JF placement contract no longer calls it unread.

## Persistent map-part assets

`RetailMission01MapRenderAssets` consumes the store-backed scene bundle and
builds the placed-city resources without filenames or TSV:

```text
models / primitives                         170 / 4,318
decoded vertices / strip indices        112,719 / 138,610
material texture references / NTXR assets 4,318 / 170
persistent draw commands                       4,226
draw classes                            345 / 584 / 3,277 / 20
```

Every primitive has one decoded mesh, normal/UV population and GIDX-resolved
texture. Every draw command addresses exactly one primitive by
`(selector, record_index)` and carries only the translation present in `.pdl`;
no rotation or substitute transform is introduced. The 92 entries rejected by
retail's kind guard are validated against their records, then excluded.

The object owns the world bundle, exposes immutable source spans for the 170
map textures, and is move-only so those spans cannot outlive a copied owner.
Texture decoding is explicit for one-time renderer upload; frame iteration does
not rebuild the registry or decode geometry.

## Validation

```text
Release configure/build                              pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb        66/66
tools/tests                                          87/87
qualified placement-to-record join               4,318/4,318
qualified immutable geometry                     4,318/4,318
qualified GIDX map textures                         170/170
sealed-cache audit                                    17/17
mission01-final-gate-v3 --require JF                    pass
mission01-playable-gate-v1 --require JF                 pass
contract addresses                                  321/321
contract derivations                             52, gaps 0
contract artefacts                          146/146 match HEAD
```

## Residual boundaries

JV is not passed. The exact city draw list is not yet composed into an accepted
CPU/GPU frame. Terrain/water immutable GPU resources, sky and tree consumers,
mapset composition, the camera group/opening-view selector and a capture audit
remain open. The old scene tools retain diagnostic cameras and are not an
accepted runtime path. JP, frontend/localisation and sustained Vulkan timing
remain later gates.
