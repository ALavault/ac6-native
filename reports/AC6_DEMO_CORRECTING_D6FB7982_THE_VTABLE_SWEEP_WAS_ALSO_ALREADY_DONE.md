# Correcting `d6fb7982` again: the vtable sweep it proposed was also already done — the verified-open frontier, built by reading every commit forward

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run. Built by reading, in
full, every report from `5dc58584` (the commit immediately before the
render-queue/state-advance/query-forcing arc begins) through `HEAD`, in
forward chronological order (`git log --oneline --reverse
5dc58584..HEAD`), and checking each report's own "Not established"
section against every later commit title in that same range — the rule
this correction adopts going forward, stated in full at the end.

## The second error

`d6fb7982` corrected `b29dcf77`'s "instrument the 9 calls" claim, and
proposed as the real next step: "sweep every RTTI-locatable vtable in the
image for a non-stub function at `+0x20`." **That sweep was also already
done**, one commit after the one `d6fb7982` itself cited (`fbd10eef`):

- `AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS.md`
  (`883d396d`) built `tools/rtti_vtable_slot_sweep.py` for exactly this
  question, ran it (`--base-class '.?AVCSwgListener@@' --slot-offset 0x20
  --stub-address 0x820AC748`), resolved 89/89 classes, and found **40 of
  89 implement a real, non-stub `+0x20` handler** — the interface is not
  dead by design. Among the 18 distinct handler functions those 40
  resolve to, exactly one (`sub_82184130`, `CModeTaskMainSelect`)
  recognizes message code 102 (`"M102"` decoded via the same CRT
  `strtoul` this report traced) — and that class is confirmed absent from
  `functions[]` in both cached 12000-tick atlases (START-at-3000 and
  neutral), on either route.
- `AC6_DEMO_FORCING_SENDMSGIS_RESULT_DOES_NOT_TRIGGER_COMPLETION.md`
  (`c73498cb`) then ran `883d396d`'s own named falsifier: force
  `SendMsgI`'s boxed return to `1` or `2` (the two values
  `CModeTaskMainSelect`'s handler can produce) and watch for
  `sub_820EA4A8`. Neither value produced a second call, in 600-tick or
  5000-tick windows.

`d6fb7982` wrote "no report found linking `0x826DF800`/this listener
interface/slot `+0x20` to that thread by address" — `883d396d` is exactly
that link, sitting one commit after the report `d6fb7982` had already
read (`fbd10eef`). Same mechanical error as before: adopting a report's
"Not established" section without checking the commits that follow it.
`b29dcf77`'s own EndMode-ruled-out finding is untouched by this
correction — both errors are next-step framings only.

## The verified-open frontier

Reading forward past `c73498cb` finds the thread did not stop there — it
continued one more level, into the specific value `GetCurrentMission`
returns:

- `AC6_DEMO_CORRECTING_GETCURRENTMISSION_IS_ALWAYS_16_THE_BRANCH_WAS_READ_BACKWARDS.md`
  (`ee81086d`) corrected an inverted branch reading three prior reports
  shared: `16` is `GetCurrentMission`'s gate-**success** value, not a
  failure fallback — the gate fails every tick in this run
  (`[gs+112+8]` never matches), so the function returns
  `sub_82095B80`'s raw per-slot value (live: `0`) instead. Reconciled
  `GetCurrentMode`/`GetCurrentLevel` the same way: live `0`/`2`, ordinary
  ("still on the title screen, mission 0, level 2") reads, not broken
  fallbacks.
- `AC6_DEMO_FORCING_GETCURRENTMISSION_TO_16_ALSO_DOES_NOT_TRIGGER_COMPLETION.md`
  (`6fc7b184`) forced `GetCurrentMission`'s boxed result to `16` (the
  now-correctly-identified success value) — no second call to
  `sub_820EA4A8`, same two window sizes as the `SendMsgI` test.

`6fc7b184`'s own "Not established" names two items. Checked against every
commit from `f7c4e68f` (the very next commit) through `HEAD` — the
campaign pivoted instead into the symbol-table/category/EndMode arc, now
closed (`270dea0e`/`b29dcf77`) — and **neither item is addressed
anywhere in that range**:

1. **`GetCurrentMode` and `GetCurrentLevel`, forced individually, were
   never tested.** Only `SendMsgI` and `GetCurrentMission` were.
2. **Forcing more than one of the four queries simultaneously, in the
   same tick-3001 batch, was never tested.** `6fc7b184`'s own text: "the
   script asks all four in the same tick-3001 batch, so a combined effect
   (e.g. it only branches once all three state queries agree) can't be
   ruled out by testing one at a time." This is the specific gap every
   single-value falsifier so far has left open by construction.

These are the genuinely verified-open items — not from reading one
report, but from reading every report between the point they were named
and `HEAD`.

## The forward-check rule, adopted going forward

**A "next step" named in any report is only open if grep across every
later commit title in the range from that report to `HEAD` turns up
nothing addressing it.** A single report's own "Not established" section
describes the frontier *as of that commit*, not the frontier now — this
campaign's chain is close to strictly linear, and the next 1-3 commits
usually already closed it. Before proposing new instrumentation, run:
`git log --oneline --reverse <candidate-commit>..HEAD` and check every
title, not just the most recent handful.

## Not established

- Whether forcing `GetCurrentMode`/`GetCurrentLevel` individually, or all
  four queries together, triggers `sub_820EA4A8` — the concrete,
  now-verified-open next experiment. Not run in this report.

## Gates

No source changed. Native gate JF, demo `ctest`, and both contract audits
verified below before commit.
