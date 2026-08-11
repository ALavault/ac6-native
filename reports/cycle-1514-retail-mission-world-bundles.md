# Cycle 1514 — common PAL mission-world hierarchy

## Delivered

- Added `RetailMissionWorldBundle`, keyed by the PAL data-table entries
  119–133 (one entry per campaign mission).
- The reader qualifies only the common bytes observed in all fifteen payloads:
  a 23-child root, map/mapset FHM children 21/22, MCA/MCD/MCI map resources,
  nested FHM map resources, and mapset NTXR resources 7–11.
- `RetailMissionBundle` now retains the matching world bundle when the entry
  exists in a full cache. Bounded scenario fixtures may still omit worlds so
  parser tests do not gain an implicit retail dependency.

This is a structural boundary. It deliberately does not infer geometry,
materials, textures, placement, or gameplay semantics from the hierarchy.

## Validation

The new unit test checks mapping rejection outside missions 1–15. Against the
qualified cache `/tmp/ac6-retail-v2-smoke` it opens and validates all fifteen
worlds:

```
reconstruction/ace-combat-6/build/ac6-retail-mission-world-bundle-tests \
  /tmp/ac6-retail-v2-smoke
retail mission world bundle missions=15
```

The cache index remains the qualified PAL index
`ca25a10fc9dbf1987d34fd755c9d842c887273e5083e8b4d644339b5a156bbde`.

## Boundary retained

The import still materialises the complete 926-entry closure, but no world
consumer has yet been connected to the live renderer or mission simulation.
Pose/camera production, terrain/material/texture decoding, objects, effects,
objectives, and the JV/JP/JG mission gates remain open.
