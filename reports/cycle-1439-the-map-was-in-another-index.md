# Cycle 1439 — the map was in another index

## Qualification

- **No Ghidra run and no oracle pass.** The extracted archive and the product's
  ports.
- No product C++ changed; ctest stays **53**. **No contract entry.**
- New: `tools/ndxr_loose_render.cpp`,
  `analysis/assets/mission01-map-parts.txt`.

## Searching by name, as the last report said to

Every `_O_OBJ_O_HIR` record name in `idx_0009` — 505 of them — and only **four**
carry `mapobj`: `mapobj_m01_l_brg1_b/n` and `brg2_b/n`. Two bridges. No terrain.

Then the thing I should have checked twenty cycles ago:

```
idx_0009          1111 files    70.6 MB   505 record names   4 mapobj
idx_0119           588 files   491.7 MB     0 record names   0 mapobj
idx_0165            11 files     2.4 MB     0
idx_0199            16 files     8.1 MB     0
idx_0210           131 files    14.6 MB    12
```

**There are five extracted indices and I had been working in one of them since
cycle 1418.** `idx_0119` is seven times larger than the one I knew about, and
its zero `_O_OBJ_O_HIR` names is why a name scan of `idx_0009` found nothing —
its records use a different suffix.

## What is in it

178 loose `.ndxr` files and 192 loose `.ntxr`, plus 534 and 567 more inside
FHM containers. Textures up to 22 MB each.

Measured with the product's own decoder — **178 files, 4,326 descriptors,
113,211 vertices** — the names are `mapparts_m01_*`:

| stem | files |
|---|---:|
| `mapparts_m01_s_…` | 71 |
| `mapparts_m01_l_…` | 45 |
| `mapparts_m01_m_…` | 43 |
| `mapparts_m01_x_…` | 10 |
| `tree…` | 8 |
| `mapparts_m01_airport_003` | 1 |

Gracemeria. Small, medium and large parts, trees, and an airport.

**Nothing exceeds ~512 units in x or z**, which makes this a tiled city rather
than one mesh, and six pieces have a y extent under 1 — flat ground plates. The
tall ones are buildings: `mapparts_m01_l_006` is 76 × **262** × 45.

`008_NDXR.ndxr` — `mapparts_m01_airport_003` — is a 512 × 6 × 512 plate whose
290 vertices draw as crossing runway strips.

## What this corrects

Cycle 1438 concluded "Mission 01's package has no terrain model in the roster at
all". That is true of `idx_0009` and it read as a statement about the mission.
The map exists; it is in a different index, in a different naming scheme, and as
178 separate files rather than one model.

Three cycles asked "where is the terrain" while looking only inside the file
that does not contain it, and the directory listing that settles it is one
`ls`.

## Not established

- **Placement.** 178 parts each ≤512 units, all in their own local space. What
  positions them is unread — the `_sh_####` and trailing numbers in the names
  may encode it, or a separate layout may.
- The airport's texture id is **4177**, a small number outside the `0x10……`
  namespace every model texture uses. So there is a second texture namespace and
  the sibling `.ntxr` files did not answer for it.
- What `idx_0119`'s 491 MB is mostly made of — the two largest files are 163 MB
  and 140 MB FHM containers, unexamined.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Place the parts.** 178 pieces on a 512-unit grid need a layout, and the two
candidates are in hand: the numbers embedded in the record names, and whatever
`idx_0119`'s two large FHM containers hold. The first is free to test — plot the
parts by the numbers in their names and see whether a city appears.
