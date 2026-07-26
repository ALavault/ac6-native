# Ordered MoveEffect/MOP timeline export

Date: 2026-07-15

## Result

The deterministic Scene export now writes two additional CSV artifacts for
every CUT:

- `cut/NNNN/move_effect_timeline.csv`: one row per serialized MoveEffect in
  original event order;
- `cut/NNNN/move_effect_frame_counts.csv`: one row per frame that has at
  least one MoveEffect, in first-command order.

`move_effect_timeline.csv` records only exact serialized/resource facts:
`command_order`, owning `FrameStart` index, `effect_id`, one-based Scene record
index, joined `E_EFFMOVE_` path, MOP `content_id`, and both opaque GYZ record
element counts.  The frame-count CSV records the number of event records and
the number of distinct resolved E_EFFMOVE resource IDs in that serialized
frame.  Neither file assigns a time scale, duration, visibility, transform,
or rendering operation.

## Corpus ordering result

All 16 effect-bearing CUTs preserve their MoveEffect rows in original CUT
event order.  They contain 60,626 rows total, and every row satisfies the
exact `effect_id - 1 == scene_record_index` join.  Each sampled serialized
frame in this corpus names distinct effect IDs, so its event count equals its
distinct resolved resource count; this is a count fact only, not an instance
lifetime claim.

| CUT | Effect frames | Events | Events/frame range | Serialized frame span |
| --- | ---: | ---: | --- | ---: |
| 0002 | 150 | 450 | 3 | 149 |
| 0003 | 120 | 2040 | 17 | 119 |
| 0004 | 140 | 420 | 3 | 139 |
| 0005 | 220 | 660 | 3 | 219 |
| 0006 | 160 | 2240 | 14 | 159 |
| 0007 | 140 | 840 | 6 | 139 |
| 0009 | 570 | 8367 | 13--16 | 569 |
| 0011 | 300 | 6600 | 22 | 299 |
| 0012 | 160 | 2400 | 15 | 159 |
| 0015 | 270 | 4050 | 15 | 269 |
| 0032 | 260 | 5738 | 22--25 | 259 |
| 0033 | 360 | 3198 | 8--11 | 359 |
| 0034 | 210 | 840 | 4 | 209 |
| 0035 | 150 | 6203 | 36--43 | 149 |
| 0036 | 420 | 12320 | 21--35 | 419 |
| 0037 | 284 | 4260 | 15 | 283 |

For CUT `0002`, the first frame serializes effect IDs 4, 5, and 7 in that
order, resolving to Scene records 3, 4, and 6. Its frame-count artifact starts
with `(command_order=0, frame=1, command_count=3, resource_count=3)`.

The two opaque record element counts are not generally equal: 33,148 of the
60,626 rows differ. This is a further reason not to convert either count into
a duration or transform interpretation.

## Validation

The exporter was rebuilt with `-j 16`, then run against a freshly extracted
retail DATA entry 9. It emitted 44 CUT directories and 553 MOP directories,
including all ordered timeline artifacts. The regenerated corpus has 60,626
MoveEffect rows, zero failed one-based joins, and the expected CUT `0002`
150-frame/450-event result. Existing isolated parser/resource tests remain
covered by the successful 36/36 AC6 suite run for the native timeline change.

## Native shell boundary

The Scene shell consumes the same exact joins only for replayable camera
groups and displays count ticks when such a group contains events. Some
effect-bearing CUTs have no bounded Tcam replay route, so the CSV export is
the authoritative ordered inspection surface for them. It does not make an
effect-rendering video claim.
