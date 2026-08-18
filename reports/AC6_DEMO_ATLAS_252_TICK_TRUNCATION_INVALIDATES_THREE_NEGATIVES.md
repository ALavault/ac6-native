# Correcting `03179c5b`, `8fba5b45`, `ce065acb`: the reachability control stopped at tick 252

## The error

`03179c5b`, `8fba5b45`, and `ce065acb` all cite
`analysis/demo/ac6-demo-menu-consumer-reach-ab/sha256/.../buttons16/buttons16.atlas.json`
(2286 functions) as a full-reachability control for claims that `sub_820C2CC0`,
`sub_8217C678`, `sub_82173DF0`, and `0x8217C4D8` are unreached.

That atlas's own metadata:

```
tick_range: {first: 0, last: 252}
replay: rtply_v4_sha256=4a7326d9..., xam_movie_v1_sha256=6ff80573...
```

It covers ticks 0-252 only — before the title screen even exists (tick 2429,
`AC6_DEMO_START_DURING_TITLE.md`), let alone the correctly-timed START press
during the title window (tick 3000, `651e7878`). Its own "buttons16" input was
the same mistimed tick-252 press `634cff33` already retired as a methodology
error for a different measurement.

`AC6_DEMO_ATLAS_NAMES_THE_TWO_SLOTS.md`, by contrast, used a **12000-tick**
atlas and reports 2711/2779 functions on the two routes — 400+ more than the
252-tick one. `ce065acb`'s subject, `sub_8217C678` (slot `+0x0C` of
`CModeTaskGameDemoOffline`), could only execute after the title hands off to
that mode task, which by construction cannot happen before tick 2429. Checking
for it in a 0-252 atlas could not have found it regardless of whether it is
actually reached later in a correctly-timed run.

## Why the sanity check in `03179c5b` didn't catch it

`03179c5b` cross-checked the atlas against three already-known-reached
functions (`sub_821B9BC8`, `sub_821AD378`, `sub_821B94A8`) and found them
present with plausible counts — real evidence the atlas isn't corrupt or
empty. But all three have `last_tick=252`: they run during early device
setup, inside the window the atlas actually covers. A cross-check built from
early-tick functions cannot detect a truncation that only drops
*later*-tick functions, which is exactly the population every negative claim
in these three reports depends on.

## What still stands, what doesn't

- `03179c5b`'s **positive** claims (the doorbell function, its two gates, the
  measured `bit1`/`f21508` values, the two tick-0 kicks) are unaffected —
  those came from a live trace and direct code reads, not this atlas.
- `03179c5b`/`8fba5b45`'s claim that `sub_820C2CC0` (the `[0x827AD2F0]` seed
  candidate) is unreached is **unsupported as stated**. It may still be
  unreached — `sub_820C2CC0`'s registration-stub shape and its callee
  `sub_8219D4E8` give no obvious reason to expect it fires only late — but
  this needs re-checking against a control that actually covers the run.
- `ce065acb`'s reachability claim about `sub_8217C678`/`0x8217C4D8` is
  **unsupported as stated**, for the strongest possible reason: the class it
  belongs to (`CModeTaskGameDemoOffline`) cannot be entered inside the
  window the control covers. The RTTI naming (`sub_8217C678` is slot `+0x0C`
  of the whole `CModeTaskGame*` family, `0x8217C4D8` is the callback it
  registers via `sub_82173DF0`) is unaffected — that came from static code
  reading, not the atlas.

## Fix in progress

A new atlas, `probe --atlas --xam-movie-record`, 12000 ticks, START pressed
at tick 3000-3001 (the timing `651e7878` established as correct), is running
in the background. The three addresses above will be re-checked against it
and either `ce065acb`/`8fba5b45` will be confirmed load-bearing or corrected
again, by name, per this file.
