# Forcing `SendMsgI`'s boxed result to 1 or 2 does not make title call the completion trigger

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: a new, opt-in,
**write-capable** intervention (`AC6_DEMO_FORCE_SWG_MSGI_RESULT`,
`guest_bridge/swg_native_call_trace.hpp`), `probe --until frontend`,
correctly-timed START at tick 3000, headless backend, no oracle. This is
the first behavior-modifying instrument alongside this campaign's
`AC6_DEMO_WATCH_*` family — every one of those only observes; this one
writes.

## What this closes

`AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS.md` traced a
complete mechanism (title's script would receive a well-typed `0` from
`SendMsgI("M102")`) but explicitly could not establish whether the script
*reads and branches on* that value, and named a live falsifier: force the
boxed result away from `0` and watch whether `sub_820EA4A8` (the
completion trigger, `642f77a4`) gets called for title. This report runs
that experiment.

## The intervention

`sub_820E9838`'s out-param address (its own `r4`, captured before the
call — `r4` is a volatile argument register the callee is free to
clobber, so it cannot be read back afterward) is saved by a new wrapper,
`invoke_body_trace_with_swg_msgi_override`, around the existing
`invoke_body_trace` call site in `AC6_PPC_CALL_INDIRECT`. After the call
returns — by which point `sub_820E9838`'s own unconditional write has
already landed — `apply_swg_sendmsgi_override` overwrites that exact
guest address with a caller-supplied value from
`AC6_DEMO_FORCE_SWG_MSGI_RESULT`, only when `lr==0x820E9130` (the
marshaller's dispatch site) and `guest_address==0x820E9838`
(`SendMsgI` specifically) — every other indirect call is untouched.
Folded into the existing `invoke_body_trace` call site rather than added
inline, since `AC6_PPC_CALL_INDIRECT` was already at its 220-line
`ac6-demo-complexity` budget.

## Calibrating the intervention before trusting it

Ran once with `AC6_DEMO_WATCH_ADDR_LO`/`HI` bracketing the exact out-param
address (`0x7F040198`-`0x7F04019C`) to confirm the forced value actually
*survives* to the point the marshaller reads it back, not just that the
write happened. The relevant slice of the trace, in order:

```
AC6_SWG_NATIVE_CALL target=0x820E9838 ... tag=M102
AC6_ADDR_RANGE_WRITE address=0x7F040198 value=0x0  lr=... sub_820E9838 (its own unconditional "handled" write)
AC6_SWG_MSGI_FORCED  address=0x7F040198 value=2                        (the override)
AC6_ADDR_RANGE_WRITE address=0x7F040198 value=0x7F0401F8  lr=... sub_820E8F90  (unrelated: a LATER, separate marshaller invocation's own setup write, for the next command — same stack address reused, not this call reading its own result)
AC6_SWG_NATIVE_CALL target=0x820EA6C0 ...
```

No write lands on that address between the override and the next
marshaller invocation's setup — and reads, by construction, produce no
trace line at all, so the read itself cannot be directly observed. What
*can* be shown is the negative: nothing overwrote the forced value before
this same `sub_820E9838` call's own `sub_820E8F90` invocation would have
read it in its Integer-boxing branch (`loc_820E91DC`,
`AC6_DEMO_M102...`'s report). The forced value reaches the read point
uncorrupted.

## The experiment, two values, two windows

`AC6_DEMO_FORCE_SWG_MSGI_RESULT=1`, `--max-ticks 3600` (600 ticks
post-press, confirmed run to completion: `probe complete; outcome=max_ticks
ticks=3600`): the trace shows the identical 9 `AC6_SWG_NATIVE_CALL` lines
this campaign has measured since `6e8fab2f`, `AC6_SWG_MSGI_FORCED
value=1` firing exactly once on title's own call, and **no second call to
`target=0x820EA4A8`** — only startup's original tick-2425 call.

`AC6_DEMO_FORCE_SWG_MSGI_RESULT=2`, `--max-ticks 8000` (5000 ticks
post-press, the same extended window `...NAMED_NONE_IS_COMPLETION.md`
used to rule out a delayed reaction; confirmed run to completion:
`probe complete; outcome=max_ticks ticks=8000`): identical result. Same 9
calls, `AC6_SWG_MSGI_FORCED value=2` firing once, no second call to
`sub_820EA4A8` anywhere in the extended window.

## Incidental catch: the query answers themselves, and a conflict with `3c7e7291`

The calibration run's write-watcher, bracketing only the `SendMsgI`
out-param address but firing on every write to it, incidentally recorded
what the other three tick-3001 queries actually wrote into that same
reused stack slot before their own results got read back:
`sub_820EA598` (`GetCurrentLevel`) wrote **2**; `sub_820EA550`
(`GetCurrentMission`) wrote **0**; `sub_820EA538` (`GetCurrentMode`)
wrote **0**. **`GetCurrentMission` writing `0` conflicts with
`3c7e7291`'s live measurement of a "fallback value **16**"** — a report
this campaign's own capstone (`...M102_RESOLVES...md`) repeats without
flagging the discrepancy. Not reconciled here: whether `16` was an
internal fallback constant read at a different point (e.g. before boxing,
or from a different field) while the out-param that actually reaches the
script carries `0`, or whether the two measurements are of different
quantities entirely. This is the necessary first check before the
query-forcing falsifier named below can be run meaningfully — "force a
correct answer" requires first knowing which of `0` or `16` (or neither)
the script actually receives today.

## Conclusion

**Forcing `SendMsgI`'s returned integer to `1` or `2` — the only two
non-zero values `CModeTaskMainSelect`'s handler, the message's one known
recognizer, can produce — causes no observable change in the
marshaller-level native-call trace, within 600 ticks (value 1) or 5000
ticks (value 2) of the press.** This falsifies the specific hypothesis
the prior report's falsifier targeted: **the returned integer is not
what gates the completion call, for these two values, within these
windows.**

What this does **not** establish, and this report deliberately does not
claim: that message 102 is irrelevant, or that no return value would ever
matter. Three readings survive, and this experiment cannot distinguish
them: (a) the swg interpreter discards `SendMsgI`'s result at this call
site regardless of value — the script may not even read it; (b) it reads
and branches on it, but the branch controls something invisible to this
report's only observable (the marshaller-level call trace) — a
UI/animation state, a variable the script itself stores without issuing
another native call, or a delayed effect past this run's ticks; (c) the
real trigger value is something other than 1 or 2 — this report tested
exactly the two non-zero outputs `CModeTaskMainSelect`'s own handler is
capable of producing, not an exhaustive scan.

## Not established

- Whether the swg interpreter reads `SendMsgI`'s result at all at this
  call site — not directly observable; only its *non-effect* on the
  native-call trace is shown.
- Whether some other forced value (not 1 or 2) changes the outcome — not
  swept.
- Whether a longer window than 8000 ticks would surface a delayed effect
  from the `value=2` run — not tested past that bound.
- What, if anything, `sub_820EA538`/`sub_820EA550`/`sub_820EA598`
  (`GetCurrentMode`/`Mission`/`Level`) would do if their own results were
  forced away from their actual values — not tested. All three are
  confirmed `'I'`-typed (`[row+8]` read directly from the static command
  table: `0x82386638`/`0x82386648`/`0x82386658`, all `"I"`, all
  `argtype="V"` matching their observed zero `arg_count`), so the same
  override mechanism generalizes to them directly — this is the leading
  candidate for the next falsifier, since `642f77a4`'s original
  upstream-branch hypothesis (a wrong mode/mission/level answer causes
  the "nothing to do" branch) regains the lead now that the
  downstream-return-value hypothesis is exonerated for these two values.
  **Prerequisite, not yet done**: reconcile the `GetCurrentMission=0`
  value this report's calibration run incidentally recorded against
  `3c7e7291`'s live "fallback value 16" — the two disagree, and which one
  the script actually receives has to be known before "force a correct
  value" has a defined meaning.

## Gates

New env var `AC6_DEMO_FORCE_SWG_MSGI_RESULT` — opt-in, unset by default.
Per resolved indirect call: two integer compares (`lr`, `guest_address`)
to identify the exact `SendMsgI` dispatch site; the `getenv`/`strtoul`
parse of the forced value is one-time static initialization, reached only
once dispatch actually matches. This is a write-capable instrument,
unlike the rest of the `AC6_DEMO_WATCH_*` family, and exists specifically
to test the named hypothesis above. Native gate JF, demo ctest, and both
contract audits verified below before commit.
