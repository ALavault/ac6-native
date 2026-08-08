# Cycle 1219 — where the derivation stands, and what is left

Twenty-seven cycles, 1192 to 1218. This is the synthesis, written because the
state is now complex enough that a reader arriving at HEAD cannot reconstruct it
from the reports in order.

## What is derived end to end and in the product

The **NDXR chain**, eleven stages, every one citing the address that establishes
it, ported to `include/ac6/retail_ndxr_container.h` and under the v4 contract as
`ndxr_container`:

| stage | address |
|---|---|
| recognise | `0x8234CA28`, predicates `0x8233EF48` / `0x8233EF68` |
| dispatch | `0x8234CB58` — three codes, all shipped files are `0x200` |
| construct | `0x82350CA0` / `0x82350C50` |
| sequence | `0x82352B88` — two of three slots are `blr` |
| header | `0x82350F08` — four section extents, body at `[+0x10] + 0x30` |
| records | `0x823556E0` → `0x823555D0` — fixed `0x30`, relocated in place |
| strings | relocation base `[obj+0x90]` |
| materials | `0x82355318` |
| textures | `0x8233EE40` → registry `0x828C8100`, keyed by `GIDX+0x08` |
| geometry | `0x82362190` — section 1 index, section 2 vertex |
| stride | `0x82345100` — `T8[0x820110F0] + T18[0x82011130]` |

The **NTXR decoder** was already ported; this session closed the join onto it.
The **stride constants left the product**: `native_geometry_raster.cpp` calls
`VertexStride()` instead of four literals, which retires cycle 1189's flag.

And the **execution path is proved**, twice: XEX entry → `main` → `0x821D5EF8`
with exactly one call site per link, and the frame loop → thirteen hops →
`8219A1B0`, the mission load call, selected by `mode = *([0x826E4EB4] + 0x78)`.

## What is closed negatively, and that is a result

- **FHM has no reader in this image.** The bytes occur zero times, on a byte scan
  with three positive controls. `tools/ac6_fhm.py`'s layout is measured and
  cannot be replaced by a reading, because the reading does not exist.
- **The Scene/CUT ownership edge does not exist for Mission 01.** Zero `0x2005`
  records in 575 across 44 groups, the class would not own a mesh anyway, and the
  namespace it reaches holds 1,106 `.mop` and no geometry. The fail-closed rule
  stands with a better reason than it had.
- **MATE is never parsed.** Three instruments, three positive controls, zero
  hits. The materials consumed are the NDXR-embedded copies.

## What is left for JV, stated without softening

1. **The FHM gap is real and is not derivable.** Going from a unit's model index
   to NDXR bytes runs MDLP → FHM → NDXR, and the middle link is measured. A
   drawn mesh will rest on that measurement or on offline extraction; either way
   the derivation chain has a hole that this image cannot fill.
2. **The player's placement is known and not applied.** Cycle 1206 found the Set
   tag-0 order is the spawn transform and cycle 1145's tag-2 reading is wrong,
   but `initial_world_position` is unchanged: Set 0's identity is convergent, not
   derived, and the `0x7D4` placement is not proven to execute at load.
3. **Terrain has no source.** 2e is refuted; if terrain comes from anywhere it is
   the scenario Obj→MDLP path, untouched.
4. **Wiring, not derivation.** The retail session already drives the rasteriser
   for markers. Feeding it real geometry is now an implementation step.

## The durable part, and it may outlast the findings

`INSTRUMENT_DISCIPLINE.md` grew from eight entries to **fourteen**, and six were
written from failures inside these twenty-seven cycles:

- **the true positive from dead code** — four findings were live instructions on
  a branch the build never takes;
- **querying only one side of a join** — three cycles published a rule correct on
  the corpus that produced it;
- **the listing is not the code** — Ghidra covers 91.5% of `.text` and is missing
  whole functions;
- **reachability by `bl` is the mirror error** — corrected in place from 10% to
  **26%** when a complete decode disagreed with the figure this file itself had
  published;
- **half a rule** — walk up to the conditional *and* down to the back-edge;
- **a control re-expressed in another language is a new control.**

Nine cycles this session corrected an earlier cycle, and **five corrected a cycle
from the same session** — 1202←1201, 1209←1208, 1215←1214, 1218←1213's figure,
and 1203 catching itself mid-cycle. That rate is not a sign of carelessness; it
is what the far-side rule costs and what it buys.

## The gates

```
ctest                                   27 tests, all passed (1 skipped, no DISPLAY)
audit --require JF                      mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_contract_artifacts (v4)       pass cited=29 match_head=29 readme_rows=36
```

No oracle was spent in any of the twenty-seven cycles.

## Named next reads

- The gate byte at `[0x8293BA10] + 0x15A946` — set to 1 at `821B9508`, four
  readers, no writer found and no positive control for the scan shape that would
  find one.
- What selects creator index 45 — the table is `0x82691AD8`, and `0x821C5E00`-ish
  is the place.
- `0x82675B80`, the third resource container, and what its vtable slot `+0x5C`
  does.
- The 77 texture references and 124 textures cycle 1210 left unmatched, which
  need the extraction pipeline rather than a byte scan.
