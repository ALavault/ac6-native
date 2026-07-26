# E_EFFMOVE resource schedule/count boundary

Date: 2026-07-15

## Result

No exact known resource schedule is encoded by either E_EFFMOVE MOP record's
`element_count`. The local corpus provides a strong negative result: the same
resolved resource can be named by one MoveEffect record in every serialized
frame of a CUT while either MOP record count is 1, matches neither occurrence
count nor span, or differs from the other record count.

This is a hard blocker against interpreting either count as an effect duration,
frame schedule, position-key count, or visibility rule.

## Safe new export metadata

Each `cut/NNNN` directory now also contains
`move_effect_resource_schedule.csv`. Its one row per cut-local effect ID
records only:

- first serialized command order;
- one-based Scene record index and joined E_EFFMOVE path;
- first/last serialized FrameStart index;
- serialized occurrence count;
- whether that resource occurs exactly once at every integer frame index in
  the observed first-to-last range;
- the two opaque MOP element counts.

The `every_frame_index_once` flag is a property of the CUT event sequence. It
is deliberately not a duration or an effect lifetime claim.

## Corpus comparison

The fresh entry-9 export has 247 cut-local resolved effect resources:

| Comparison | Count |
| --- | ---: |
| Resources with every serialized frame index present once | 241 |
| Resources with a gapped or repeated serialized schedule | 6 |
| Record 0 count equals serialized occurrences | 0 |
| Record 0 count is occurrences plus one | 28 |
| Record 1 count equals serialized occurrences | 1 |
| Record 1 count is occurrences plus one | 10 |

Concrete counterexamples make the boundary decisive:

- CUT `0003`, effect ID 3, is serialized in each frame 1--120 (120
  occurrences), while both MOP counts are 1;
- CUT `0005`, effect ID 5, is serialized in each frame 1--220 (220
  occurrences), while its counts are 217 and 221;
- CUT `0005`, effect ID 7, shares the identical 220-frame schedule but has
  counts 201 and 221.

Thus even equal schedules do not determine either count, and equal counts do
not determine the schedule.

## Validation

`ac6-scene-remaster-export` was rebuilt with `-j 16` and run against a fresh
local extraction of retail DATA entry 9. It emitted 44 CUT and 553 MOP
directories. The schedule report contains 247 resolved resource rows; CUT
`0005` emits the three expected resources (effect IDs 4, 5, 7) with 220
serialized occurrences each. The existing MOP/NFIC/Scene test subset passed.

No effect rendering or shell behavior was changed by this export-only pass.
