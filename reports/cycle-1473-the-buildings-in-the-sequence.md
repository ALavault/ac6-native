# Cycle 1473 — the buildings in the sequence

## Qualification

- **No Ghidra run and no oracle pass.** The product and the map container.
- Product C++ **changed**: `draw_world_triangles` added to `demo_flight_view`.
  ctest stays **59**.
- **No contract entry.** The light, the culling range and the camera are mine.

## What is in the frame now

The flight sequence draws the ground, then the placed parts on top of it,
depth-tested against it. 3,600 ticks, 1,800 frames, from `(1000, 420, -24000)`
inland to `(2215, 420, 961)` over the bay — the city, its streets and the
bridge across the bay mouth, from the aircraft's own eye.

`reports/mission01-terrain/mission01-flight-with-city.mp4` — full path:
`/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/reports/mission01-terrain/mission01-flight-with-city.mp4`,
in the repository and not a scratch directory.

The projection lives in the library, not the tool: `draw_world_triangles` takes
world-space triangles and uses the same `to_camera`/`project` pair everything
else in that file uses. Flattening the parts stays in the caller, because the
placement, the model cache and the culling belong to it.

**2m48s for 1,800 frames**, with 170 models decoded once and reused across every
frame and every instance. Without that cache it would be 1,800 × 4,318 NDXR
opens.

## What the reviewer said, and both are right

> "Il manque un très grand nombre d'assets. Et il manque une skybox."

**The assets.** This cycle adds the buildings — the previous video had none, since
`draw_terrain_view` draws ground and water only. What is still missing is
larger than what was added: no textures (the NTXR decode exists and is
contracted, and nothing here samples it), no vegetation, no sea surface, and
**two containers in this very FHM have never been opened** — `015_FHM` at 15 MB
and `016_FHM` at 140 MB, listed as open defects since cycle 1445.

**The skybox.** There is no sky here at all, only a flat fill. And the evidence
for a real one is already in hand and was walked past twice:

- `CSkySphere`, vtable `0x8205CAEC`, in the class map — it is the entry
  immediately after `CMapManager`'s vtable, and cycle 1463 read the words
  around it while looking for something else;
- the map loader's own string table at `0x8205BE3C` onward:
  `sph%s.sph`, `sphdef.sph`, `envsmap%s`, `envsmapdef`, `envcmap%s`,
  `envcmapdef` — a sky sphere and two environment maps, named by retail, in the
  table cycle 1454 dumped in full.

Neither is speculative and neither has been read.

## Not established

- Everything above. Nothing about `CSkySphere` or the `.sph` files has been
  read; they are named leads, not findings.
- Whether the parts drawn are the right ones. Cycle 1454 settled the selector
  from the loader, and the pictures are consistent with it, but no cycle has
  checked a named building against its name.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**The sky, because it is named and cheap.** `CSkySphere` has a vtable and the
loader has `sph%s.sph`; the same route that took `CMapManager` from an unnamed
function to a decoded heightfield — find the constructor, read the loader, find
the file — applies unchanged, and the answer is a dome instead of a flat fill.
