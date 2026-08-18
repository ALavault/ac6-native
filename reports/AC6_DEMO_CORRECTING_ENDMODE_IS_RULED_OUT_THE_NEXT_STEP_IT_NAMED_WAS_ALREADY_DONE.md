# Correcting `b29dcf77`: the next step it named was already done, twice, before this session's EndMode work even began

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run. Re-reading two existing
reports (`AC6_DEMO_TITLES_FIVE_POST_START_NATIVE_CALLS_NAMED_NONE_IS_COMPLETION.md`,
`AC6_DEMO_EVERY_REGISTERED_LISTENER_AT_THE_MOMENT_OF_SENDMSGI_IS_DEAD.md`)
that predate this session and were not checked before `b29dcf77` was
written.

## The error

`b29dcf77` closed by naming "the concrete next step" for the campaign's
primary frontend/state-advance thread: "instrument `sub_820E8F90`'s 9
calls (ticks 1045-3001) for their actual command tags/arguments... rather
than continuing to examine EndMode's own statement." That work already
existed, complete, with every one of the 9 calls named, before this
session's EndMode arc (`1fcc88b3` onward) even started:

- `AC6_DEMO_TITLES_FIVE_POST_START_NATIVE_CALLS_NAMED_NONE_IS_COMPLETION.md`
  (commit `6e8fab2f`) built exactly this instrument
  (`AC6_DEMO_WATCH_SWG_NATIVE_CALL`, `trace_swg_native_call`, already
  present in `swg_native_call_trace.hpp`) and named all 9 calls by target,
  `table_row`, `context`, `arg_count`, and `first_arg` — the exact table
  reproduced in `b29dcf77`. It identified `sub_820EA538`=`GetCurrentMode`,
  `sub_820EA550`=`GetCurrentMission`, `sub_820EA598`=`GetCurrentLevel`,
  `sub_820E9838`=`SendMsgI` (tag `"M102"`, a well-formed, validated
  message), and `sub_820EA6C0` as an audio-log utility. None is
  `sub_820EA4A8`, the completion trigger.
- `AC6_DEMO_EVERY_REGISTERED_LISTENER_AT_THE_MOMENT_OF_SENDMSGI_IS_DEAD.md`
  (commit `fbd10eef`) went further: walked the full 16-slot listener array
  at the moment `"M102"` is sent and found both registered listeners
  (title's own, and a newly RTTI-identified `CSelectMessageDlgManager`)
  share the same dead `+0x20` stub `"M102"` is sent through.

This session's `b29dcf77` presented the marshaller-call question as
unattempted. It was not — this is the same failure mode CLAUDE.md and this
session's own `ab3aed60` both name: proceeding on an assumption instead of
checking. No source or prior conclusion is wrong here (the EndMode-ruled-out
finding itself stands), only the "next step" framing.

## The real, still-open next step, precisely as `fbd10eef` left it

`fbd10eef`'s own "Not established" section named it and did not attempt
it: **sweep every RTTI-locatable vtable in the demo image for a non-stub
function at the `+0x20` position of this specific 22-slot listener
interface** (the interface both `CModeTaskTitleDemoOffline`,
`CModeTaskStartUpDemoOffline`, and `CSelectMessageDlgManager` all
implement identically, stubbing `+0x20` at `0x820AC748`). Two outcomes,
as `fbd10eef` itself framed them:

- **Zero implementors anywhere in the image** → "dead by design" becomes
  the stronger reading, and weight shifts back to the upstream hypothesis
  (title's script takes a branch that never should have sent `"M102"`
  expecting a real recipient).
- **Any real implementor found** → "a working recipient exists but isn't
  reached [i.e., isn't constructed/registered in this offline/no-mission
  scenario]" becomes the live question — which would tie directly into
  the campaign's standing mission-scoped-precondition explanation
  (`CX360UnitManager` et al.) the same way this session's own EndMode
  falsifier did.

Not yet checked whether a `CX360UnitManager`-family class is such an
implementor — no report found linking `0x826DF800`/this listener
interface/slot `+0x20` to that thread by address. That link, if it
exists, is unread, not established, and not claimed here.

## Not established

- The sweep itself — not run in this report.
- Whether any `CX360UnitManager`-family class registers into
  `0x826DF800` or implements this interface at all.

## Gates

No source changed. Native gate JF, demo `ctest`, and both contract audits
verified below before commit.
