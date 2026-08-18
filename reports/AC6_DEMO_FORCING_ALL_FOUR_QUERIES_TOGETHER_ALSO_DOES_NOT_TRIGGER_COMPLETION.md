# Forcing all four queries together — `SendMsgI`, `GetCurrentMission`, `GetCurrentMode`, `GetCurrentLevel` — also does not trigger completion

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: two runs (`--max-ticks
3600` and `--max-ticks 8000`, 600 and 5000 ticks post-press respectively),
correctly-timed START at tick 3000, headless backend, no oracle. Two new,
opt-in, write-capable instruments (`AC6_DEMO_FORCE_SWG_MODE_RESULT`,
`AC6_DEMO_FORCE_SWG_LEVEL_RESULT`), extending the existing
`apply_swg_sendmsgi_override`/`apply_swg_mission_override` pattern to
`GetCurrentMode` (`sub_820EA538`) and `GetCurrentLevel` (`sub_820EA598`),
all four applied in the same run.

## What this closes

`AC6_DEMO_CORRECTING_D6FB7982_THE_VTABLE_SWEEP_WAS_ALSO_ALREADY_DONE.md`
built the verified-open frontier by reading every commit from `5dc58584`
to `HEAD` forward: `c73498cb` forced `SendMsgI`→1/2 alone (no effect),
`6fc7b184` forced `GetCurrentMission`→16 alone (no effect), and both left
two items untested — `GetCurrentMode`/`GetCurrentLevel` forced
individually, and all four forced **together**, explicitly named as
untestable one at a time ("the script asks all four in the same tick-3001
batch, so a combined effect... can't be ruled out"). This report runs the
combined case directly, covering both gaps in one experiment.

## The instrument

`invoke_body_trace_with_swg_msgi_override`
(`swg_native_call_trace.hpp`) already captured the shared out-param
convention (all four queries box their result through `r4`, confirmed by
direct read of all four handlers' generated bodies across this and prior
reports). Extended its two-way `is_send_msg_i`/`is_get_mission` dispatch
to four: added `is_get_mode` (`guest_address==0x820EA538U`) and
`is_get_level` (`guest_address==0x820EA598U`), each gated on the same
`lr==0x820E9130U` marshaller dispatch site, each with its own
`apply_swg_mode_override`/`apply_swg_level_override` function (identical
write-only, one-time-`getenv` shape as the two existing overrides). No
new call sites; `AC6_PPC_CALL_INDIRECT`'s own single call into this
wrapper is unchanged.

## The values chosen, and why they're exploratory

No derived "healthy" sentinel exists for `GetCurrentMode`/`GetCurrentLevel`
the way `16` is `GetCurrentMission`'s confirmed gate-success value
(`ee81086d`) or `1`/`2` are the only two values `CModeTaskMainSelect`'s
handler can produce (`883d396d`) for `SendMsgI`. `GetCurrentMode` is a
direct, gate-free read of `[gs+120]` (live: `0`); `GetCurrentLevel` remaps
only `6↔7` around a per-slot default-path read (live: `2`). Forced both
away from their live values to `1` — the smallest change that still
differs from the observed baseline — alongside `GetCurrentMission=16` and
`SendMsgI=2` (both already individually null, retained here for
completeness of the combined test, not because either alone is expected
to matter). This is one exploratory combination, not a sweep of the value
space; see "Not established."

## Calibration and result

Both runs' traces show the identical 9 `AC6_SWG_NATIVE_CALL` lines this
campaign has measured since `6e8fab2f`, with all four forced values
landing at the correct call, in order, at tick 3001:

```
target=0x820EA598 (GetCurrentLevel)   -> AC6_SWG_LEVEL_FORCED   value=1
target=0x820EA550 (GetCurrentMission) -> AC6_SWG_MISSION_FORCED value=16
target=0x820EA538 (GetCurrentMode)    -> AC6_SWG_MODE_FORCED    value=1
target=0x820E9838 (SendMsgI, "M102")  -> AC6_SWG_MSGI_FORCED    value=2
target=0x820EA6C0 (audio log)         -> (untouched)
```

**Neither run shows a second call to `target=0x820EA4A8`** — only
startup's original tick-2425 call appears, in both the 600-tick and the
5000-tick window. Both runs completed cleanly (`outcome=max_ticks`,
`ticks=3600`/`8000`, `milestone_reached=false`) — not killed by a probe
timeout (the first attempt at `--max-ticks 8000` with the default 300s
wall-clock budget genuinely ran out of real time before finishing,
`return_code=124`; re-run with a 590s budget completed normally,
recorded here as a process note, not a finding).

## Conclusion

**Forcing all four of title's post-press native queries simultaneously to
plausible non-default values produces no observable change in the
marshaller-level native-call trace, in either a 600-tick or a 5000-tick
post-press window.** This closes the specific combined-effect gap
`6fc7b184` left open. Combined with every single-value test already run
(`SendMsgI`→1/2, `GetCurrentMission`→16), the leading remaining readings
from `AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS.md` are
narrowed further: it is not merely that no single query's answer gates
the completion call, and now also not that some combination of these
specific four values does either.

## Not established

- Whether the swg interpreter reads any of these four return values at
  all — still not directly observable, same limitation as every prior
  report in this thread.
- Whether some *other* combination of values (not `1`/`16`/`1`/`2`)
  triggers the call — this report tested one specific combination, not an
  exhaustive sweep of a 4-dimensional value space.
- Whether a window longer than 8000 ticks surfaces a delayed reaction —
  not tested past that bound.
- Given the query-forcing thread (single and combined) is now exhausted
  of its concrete, well-motivated candidate values without result, the
  campaign's own standing alternative reading gains weight again: the
  branch that decides whether to call `sub_820EA4A8` may not key on these
  four return values at all — the actual discriminator may be somewhere
  else in the script bytecode this campaign has still not located (the
  compiled clip data itself, per every report since `b67e7f6f`).

## Gates

Source changed: `swg_native_call_trace.hpp` gained two new opt-in,
write-only instruments (`AC6_DEMO_FORCE_SWG_MODE_RESULT`,
`AC6_DEMO_FORCE_SWG_LEVEL_RESULT`), identical shape to the two existing
ones, no effect unless set. Both build trees rebuilt. Native gate JF,
demo `ctest` (26/26), and both contract audits verified below before
commit.
