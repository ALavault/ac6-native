# Forcing `GetCurrentMission`'s boxed result to 16 does not trigger completion either

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `probe --until frontend
--max-ticks 8000`, `--store` freshly copied from the known-good neutral
store, headless backend, no oracle, correctly-timed START (tick 3000).
Instrument: the write-capable override family extended this cycle
(`AC6_DEMO_FORCE_SWG_MISSION_RESULT`, `guest_bridge/swg_native_call_trace.hpp`),
alongside the existing `AC6_DEMO_FORCE_SWG_MSGI_RESULT`.

## What this closes

`AC6_DEMO_CORRECTING_GETCURRENTMISSION_IS_ALWAYS_16_THE_BRANCH_WAS_READ_BACKWARDS.md`
(`ee81086d`) reconciled a backwards branch read: `GetCurrentMission` always
returns `sub_82095B80`'s raw computed value (`0`, live) in this run, because
its gate (`sub_820E9300`) always fails — `16` is the value the gate's
*success* path would substitute, never observed live, and named as the
natural next probe. This report runs that probe.

## The intervention

Generalized the existing SendMsgI override wrapper
(`invoke_body_trace_with_swg_msgi_override`) rather than duplicating it: it
now captures `r4` (the out-param address, per the marshaller's uniform
box-through-r4 convention — confirmed directly in `sub_820EA538`'s own body,
`PPC_STORE_U32(ctx.r4.u32+0, ...)`) for *either* `guest_address==0x820E9838U`
(SendMsgI) or `guest_address==0x820EA550U` (`GetCurrentMission`), same
dispatch site (`lr==0x820E9130U`). A new sibling function,
`apply_swg_mission_override`, gated on `AC6_DEMO_FORCE_SWG_MISSION_RESULT`,
mirrors `apply_swg_sendmsgi_override`'s shape exactly. No change to
`AC6_PPC_CALL_INDIRECT`'s own line count in `guest_bridge.cpp` — the call
site is unchanged, all new logic lives in the header.

## Build note

The rebuilt-and-relinked `recompilation/ace-combat-6-demo/build` runtime
library does **not** carry the codegen-linked guest — a probe run against
`build/ac6-demo-recomp` traps immediately with `"generated guest is not
linked in this build"`. The probe binary that actually executes guest code
lives in `build-codegen-on/`, a separate CMake configuration; both must be
rebuilt (`touch src/guest_bridge.cpp` first, per the standing ninja
header-dependency gap) after any `guest_bridge/*.hpp` change before a probe
run, not just the gate-tested `build/` tree.

## The experiment

`AC6_DEMO_FORCE_SWG_MISSION_RESULT=16`, `--max-ticks 8000` (5000 ticks
post-press): confirmed run to completion (`probe complete;
outcome=max_ticks ticks=8000`, `presents=7863`). The override fired exactly
once, on the correct out-param address, at the expected tick:

```
AC6_SWG_MISSION_FORCED tick=3001 address=0x7F040198 value=16
```

(`0x7F040198` is the same reused stack slot address `c73498cb`'s calibration
run already confirmed the marshaller reads back from, for this same call
site.)

The native-call trace is **identical to every prior run in this campaign**:
the same 9 `AC6_SWG_NATIVE_CALL` lines, `target=0x820EA4A8` (the completion
trigger) appearing exactly once — startup's own tick-2425 call. No second
call, for title or anyone else, anywhere in the 5000-tick post-press window.

## Conclusion

**Forcing `GetCurrentMission`'s returned integer to `16` — the one value
this campaign had never observed live, and the value its own gate function
would produce on success — causes no observable change in the
marshaller-level native-call trace, within 5000 ticks of the press.** This
extends `c73498cb`'s negative result (forcing `SendMsgI`'s result to `1` or
`2`) to the third query in the same tick-3001 batch: none of the three
"could this value be what the script needs" hypotheses tested so far
(`SendMsgI=1`, `SendMsgI=2`, `GetCurrentMission=16`) changes anything.

This does not establish that `GetCurrentMission`'s value is irrelevant —
the same three readings `c73498cb` named for `SendMsgI` apply here
unchanged: the script may not read the value at all at this call site; it
may read it but branch on something invisible to the native-call trace; or
the value that matters is neither `0` (the value in every other run) nor
`16` (the only other value this dispatcher's own code path can produce for
this state) — `GetCurrentMode` and `GetCurrentLevel`'s own values, or some
combination across all three, remain untested.

## Not established

- Whether `GetCurrentMode` or `GetCurrentLevel`'s results, forced away from
  their live `0`/`2`, would change anything — not tested; the same override
  mechanism generalizes to both directly (both confirmed `'I'`-typed via the
  static command table, same marshaller convention) and is the natural next
  extension of this same instrument.
- Whether forcing more than one of the four queries (`GetCurrentMode`,
  `GetCurrentMission`, `GetCurrentLevel`, `SendMsgI`) simultaneously, in the
  same run, produces an effect that forcing each alone does not — not
  tested; the script asks all four in the same tick-3001 batch, so a
  combined effect (e.g. it only branches once all three state queries agree)
  can't be ruled out by testing one at a time.
- Whether a window longer than 8000 ticks surfaces a delayed reaction to
  this specific forced value — not tested past that bound, though
  `AC6_DEMO_TITLES_FIVE_POST_START_NATIVE_CALLS_NAMED_NONE_IS_COMPLETION.md`
  already established no delayed reaction for the unforced baseline out to
  the same bound.
- Whether `[gs+112+0x206E4]` (the slot selector both `sub_82095B80` and
  `sub_820E9290` clamp and index by) changing would itself alter what
  `GetCurrentMission`/`GetCurrentLevel` return without any override — not
  read or tested; forcing the marshaller's boxed *output* is a different
  intervention point than changing the state block's own input field.

## Gates

New env var `AC6_DEMO_FORCE_SWG_MISSION_RESULT`, opt-in, unset by default,
same shape as the existing `AC6_DEMO_FORCE_SWG_MSGI_RESULT`. Per resolved
indirect call: two additional integer compares (`lr`, `guest_address`) to
identify `GetCurrentMission`'s dispatch site; parse and store happen only
when both match. Native gate JF, demo `ctest` (26/26), and both contract
audits verified below before commit.
