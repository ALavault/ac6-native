# Cycle 1498 — the world has no filenames

## Qualification

- Target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`; no Ghidra or oracle run
  occurred. Existing Ghidra changes and untracked scripts were preserved.
- Entry 119 came only from the external sealed cache, index
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
  No retail bytes or cache path entered the commit.

## Store-backed world hierarchy

`RetailFhmView` is the bounded, non-owning nested form of the existing ports of
`0x82234C18` and `0x82234DD0`. It validates the native endian/version, the four
parallel arrays and every live child extent before exposing a span. Empty slots
remain distinguishable from invalid indices, and a descendant cannot pass
through an opaque child.

`RetailMission01SceneBundle::open(store)` requires the exact qualified payload
SHA-256 of `DATA.TBL[119]`, retains the content-index identity and resolves the
retail hierarchy by child indices only:

```text
root 21        map FHM, 17 slots
  1/2/3        MCA/MCD/MCI water triple
  4/5          terrain grid and 74-patch heightfield
  9/10         terrain atlas selectors
  11           4,318 map placements
  12/13        placed-tree and procedural-tree resources
  14/15        256-slot parallel model/texture FHMs, 170 live each
  16           eight-slot terrain atlas FHM, seven live pages
root 22        mapset FHM, 12 slots
  5/6          eight NDXR and eight parallel NTXR
  7..11        five additional NTXR
```

The accepted store path opens `TerrainField`, `MapWaterGrid` and
`MapPlacement`; a structural payload overload is explicitly marked for tests
and carries no store provenance. One initial assertion treated the MCI bytes
`13 00` as count 19. The existing reader and corpus test correctly read the
big-endian halfword `0x1300` = 4,864; the scene bundle now pins that actual
value rather than weakening the check.

## Complete NDXR → material → GIDX → NTXR binding

The native NTXR reader now exposes the registry key at `GIDX+0x08`. It follows
the variable descriptor to the established `eXt\0` then `GIDX` relative layout
and rejects a missing, duplicate, zero or out-of-range chunk.

The scene registry covers all 192 NTXR wrappers in entry 119: two root
textures, 170 map-part textures, seven terrain pages, eight mapset textures and
five direct mapset textures. Every identifier is unique. The binding audit then
walks all 178 NDXR files with the native container/material readers:

```text
NDXR files                 178
records/descriptors        4,326 / 4,326
material slots             4,326
texture references         4,326
unique referenced GIDX     178
available NTXR GIDX        192
missing GIDX               0
unbound descriptors        0
```

The product store opener fails unless this report is complete. The diagnostic
scene renderer's filesystem enumeration and extracted filenames are no longer
needed to obtain these resources or bindings.

## Validation

```text
Release build                                     pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb     66/66
tools/tests                                       87/87
nested FHM and GIDX negative controls              pass
qualified entry-119 typed readers                  pass
qualified NDXR/material/GIDX/NTXR audit            pass, 4,326/4,326
independent sealed-cache audit                     pass, 17/17
mission01-final-gate-v3 --require JF               pass
mission01-playable-gate-v1 --require JF            pass
contract addresses                                pass, 321/321
contract derivations                              pass, 52/0
contract artefacts                                pass, 146/146 match HEAD
```

## Residual boundaries

JV is not passed. The resource graph is now native and complete, but the
accepted runtime does not yet compose it into one frame. The next step is to
build immutable render geometry/texture resources from this bundle and connect
retail placement plus the still-unresolved camera group/opening view. Tree
model selection and sky/mapset consumers remain open; light/fog approximations
must stay explicit. JP, frontend/localisation and Vulkan performance remain
later gates.
