# `3859edac`'s retracted negatives, re-checked and confirmed

## Qualification

AC6 demo PAL, same XEX SHA-256 as every report in this chain. New atlas:
`probe --atlas --xam-movie-record`, `--until frontend --max-ticks 12000`,
START pressed at tick 3000-3001 (the timing `651e7878` established as
correct). `rc=4`, `tick_range {first:0, last:11999}`, **2779 functions** —
matching `AC6_DEMO_ATLAS_NAMES_THE_TWO_SLOTS.md`'s own count for the START
route exactly, on an independently regenerated file.

## Control check

`sub_821B9BC8` and `sub_821AD378` — known reached, cited in `03179c5b` — are
present with `last_tick=11999` and counts `47238`/`11863` (up from
`250`/`116` on the truncated atlas, consistent with the run now covering
11999 ticks instead of 252). This atlas is not another truncation.

## Re-checked addresses

```
0x820C2CC0  (827AD2F0's candidate seed writer, 8fba5b45)         absent
0x8219D4E8  (its registrar callee)                                absent
0x8217C678  (CModeTaskGameDemoOffline slot +0x0C, ce065acb)       absent
0x82173DF0  (the callback-registration helper it calls)           absent
```

All four are still unreached, now against a control that actually covers the
title screen, the correctly-timed START press, and 8500 ticks past it.

## Standing

`3859edac` was right to retract the claims as *stated* — the 252-tick atlas
could not support them regardless of the true answer. It did not show the
answer was wrong; it showed the answer was unsupported. This report supplies
the support. `8fba5b45` and `ce065acb`'s conclusions stand as originally
written; no further correction to either is needed.

## Gates

No source changed; report-only commit.
