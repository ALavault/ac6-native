# Cycle 1474 — the mapset

## Qualification

- **No Ghidra run and no oracle pass.** The image, the archive, the product.
- Product C++ **changed**: `draw_world_triangles_textured` and
  `draw_terrain_view_over` added. ctest stays **59**.
- **No contract entry.** Nothing new is derived from instructions except the
  loader path below, and nothing is ported from it yet.
- New: `tools/mission01_mapset_params.py`, `tools/mission01_scene_render.cpp`.

## The reviewer was right twice

> "Il manque un très grand nombre d'assets. Et il manque une skybox."
> "It's cheap; and it look like crap."

All three are accurate, and the reasons turned out to be findable in one cycle.

## Where the sky comes from

`0x820FC8E0` in the map loader formats **`/map/mapset`** and `0x820FC900`
formats **`sph%s.sph`** under it; `0x820FC934` falls back to **`sphdef.sph`**
when the named one is absent, and `0x820FC950` memcpys the blob into
`[this+0x6840]` with its size at `+0x6844`.

So there is a **second container beside the map**, and in `idx_0119` it is
`022_FHM`.

## What was in it

`022_FHM` holds two UTF-8 XML documents, three small binaries and five NTXR.
The XML is a scene-parameter file — **433 named values in thirteen groups**:

```
sky1=127  sky2=127  tree=59  LensFlare=40  A=14  B=15  debug=14
LevelCorrection=15  HDR=10  mapparts=5  Vignetting=3  Saturation=2  player=2
```

Including, in plain text:

| | |
|---|---|
| `.sky1.sun.lrx` / `.lry` | **40** / **145** degrees |
| `.sky1.fog.far` / `.density` | **24000** / **0.014** |
| `.mapparts.distanceL/M/S` | **16000 / 12000 / 10000** |

**Cycle 1442 found `.mapparts.distanceL` in the executable's string table** and
concluded the `_l_`/`_m_`/`_s_` suffixes are draw-distance classes. It could not
say what the distances were, because the executable holds the names and the
archive holds the values. Here they are.

The renderer's invented light, invented haze and invented 6,000-unit cull are
replaced by these. `mission01-scene-retail-sun.png` is the result: the sun on the
bridge cables and the whole skyline across the bay at retail's own 16,000.

## And where the textures were

`014_FHM` holds 170 `.ndxr` and 86 `.bin` — **and no `.ntxr` at all**, which is
why the first render reported "3471 instances drawn, 0 with a texture".

`015_FHM` holds **170 `.ntxr` and 86 `.bin`**. It mirrors `014_FHM` entry for
entry. It has been an open defect — "the two largest FHM containers unexamined" —
since cycle 1445, and the parts' textures were in it the whole time.

The join is exact: of the **170** distinct texture ids the 170 models request
through `Material → TextureRef`, **170 are present** as GIDX identifiers in
`015_FHM`/`016_FHM`. Overlap 170 of 170, not one missing.

## Why it is still beige

All **177 of 177** wrappers refuse with `NtxrRefusal::PayloadSizeMismatch`.

One cause, unanimous. That is the shape of a layout rule this decoder does not
have, not of corrupt data — and it is the same class as the defect cycle 1435
fixed for the model package, where 4 of 86 decoded until array 1's padding was
understood and 82 did. This container needs the same treatment and did not get
it here.

So "it looks like crap" now has a single, precise cause instead of a shrug.

## Not established

- The `.sph` format. The loader memcpys it; nothing has read it.
- The five NTXR in `022_FHM` (`envsmap`, `envcmap`, the sky texture), and the
  three small binaries — one of which is 1,654 bytes beginning `1500.0, 1600.0,
  1500.0, 1600.0` as floats.
- Whether `.sky2`'s 127 values are a second time of day.

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

**The payload size rule for the map's textures.** 177 refusals, one cause, and a
precedent: cycle 1435 solved the same refusal for the model package by computing
the padded extent instead of trusting the file length. The map's wrappers want a
different rule and the refusal names exactly where to look.
