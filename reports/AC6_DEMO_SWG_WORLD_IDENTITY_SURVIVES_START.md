# The swg world/clip identity is unchanged after START stalls the title

## Qualification

Same target as `651e7878` (AC6 demo PAL, same XEX SHA-256). Continues
`AC6_DEMO_START_SUPPRESSES_THE_ATTRACT_ADVANCE.md`'s open question directly:
"why does the film stop calling `0x8218AB98` after the press — waiting on a
resource the port doesn't provide, or did it change screen and the next
screen fails to construct?"

## The measurement

`probe --until frontend --max-ticks 12000`, `AC6_DEMO_WATCH_MODE_STATE=1`,
`--input-at 3000,16,...` / `--input-at 3001,0,...` (the same press-and-release
pattern as `651e7878`). The run was interrupted by a local timeout at tick
6000 (real time, not guest time — `--max-ticks 12000` needs longer than the
default 5-minute budget on this box); the trace up to tick 6000 is intact and
already covers 3000 ticks past the press.

`AC6_MODE_INNER` reproduces `651e7878` exactly: state reaches 1 at tick 2452
and never advances (no further `AC6_MODE_INNER`/`AC6_MODE_SWITCH` line
through tick 6000).

`AC6_SWGW`'s `world`/`anim`/`player` pointers, sampled every 500 ticks and on
every reached-a-different-value event:

```
tick 2452..6000: world=0x2E3E3AD4 anim=0x2E3DF0D4 player=0x2E3DFA94
```

Unchanged across the press at tick 3000 and for 3000 ticks after it — same
three object addresses before, during, and long after. Only `g44` (already
known incidental, per the same field's behavior in the neutral route)
cycles.

## Reading

If the film had changed screens and the next screen's construction stalled,
the swg engine reallocating a new world/clip for that screen is the more
likely shape — a stalled construction usually shows as a *new*, half-built
object, not the *same* object standing still. The world staying byte-for-byte
identical for 3000 ticks is more consistent with "the current screen is
waiting on a resource the port doesn't provide" than with "the screen changed
and its successor never finished building."

This is suggestive, not conclusive — the swg engine could plausibly reuse one
world object across a screen transition without reallocating it, which would
produce the same observation under hypothesis (b) too. It narrows the two
readings `651e7878` left open; it does not close either one.

## Still open

The same three unknowns `651e7878` named: what specifically the film is
waiting for, whether it is XAM-notification-shaped or something else, and
what the discriminating experiment for that is. This report only rules
*against* "a new, unfinished screen" as the visible cause — it does not
identify the resource.

## Gates

No source changed; this is a report-only commit, gates already green from
`8fba5b45` earlier this cycle-block.
