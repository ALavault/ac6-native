# Correcting `CATEGORY_1_IS_STRUCTURALLY_ABSENT`: title IS bound to `EndMode`, as category 3

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: same `probe --until
frontend --max-ticks 3200` instrument as `1e90f723`, extended with two new
value-object fields (`value0`, `value12`) and the context's own vtable
pointer, run once more covering both startup's and title's contexts in the
same run (the control `1e90f723` itself never ran). Static: direct
byte-level reads of `.build/Default.xex.base.bin`'s command table and both
observed vtables, decoding every category to its real function name.

## What this corrects, and how it was caught

Advisor flagged, immediately after `1e90f723` committed, that the report's
central claim rested on a dump method never shown to find category 1
where it demonstrably exists — startup's context, whose successful
category-1 lookup and resulting call this campaign already measured live,
had never actually been dumped (the dump gate lived inside the
tick-windowed lookup-line block, and startup's context is only ever an
argument to `sub_820DFFB8` at ticks 1045/2425, both outside the
`[2990,8000]` window). Running the control: **startup's context DOES
register category 1**, `type_tag=2`, exactly as expected — the method is
sound. But decoding the new `value12` field (present at `[node+28]+12`,
where `[node+28]` is the same value-object pointer the type tag lives in)
against the static command table revealed the real error, one level up
from the method itself.

## `value12` is the command-table index; category numbers are per-instance, not global

`sub_820E8F90` derives its dispatch row as `table_base +
command_index*16` (`69ff833a`, long established). `value12` turns out to
*be* that `command_index` directly. Solved for `table_base` from three
independent pairs and verified against five more — eight exact matches,
zero misses:

```
table_base = 0x82386408

startup category=1  value12=0x22 -> row 0x82386628 -> "EndMode"           native=0x820EA4A8
startup category=2  value12=0x1C -> row 0x823865C8 -> "GetBGMTrackNo"     native=0x820EA298
startup category=4  value12=0x18 -> row 0x82386588 -> "PlayBGM"           native=0x820EA128
title   category=3  value12=0x22 -> row 0x82386628 -> "EndMode"           native=0x820EA4A8
title   category=4  value12=0x25 -> row 0x82386658 -> "GetCurrentLevel"   native=0x820EA598
title   category=5  value12=0x24 -> row 0x82386648 -> "GetCurrentMission" native=0x820EA550
title   category=6  value12=0x23 -> row 0x82386638 -> "GetCurrentMode"    native=0x820EA538
title   category=0x13 value12=0x16 -> row 0x82386568 -> "OnVoice2D"       native=0x820EA6C0
title   category=0x17 value12=0x07 -> row 0x82386478 -> "SendMsgI"        native=0x820E9838
title   category=0x18 value12=0x00 -> row 0x82386408 -> "SendMsgV"        native=0x820E93F8
title   category=0x19 value12=0x14 -> row 0x82386548 -> "SetBuffer"       native=0x820EA0A8
title   category=0x1A value12=0x1B -> row 0x823865B8 -> "StopBGMFadeOut"  native=0x820EA238
```

**Title's category `3` decodes to the identical function as startup's
category `1`: `EndMode`, `native=0x820EA4A8` — row-for-row, byte-for-byte
the same command-table entry.** `33b549ef`'s framing ("category is the
native-function ID") is corrected: **`category` is a per-script-instance
local symbol index into that script's own bound-import table — not a
global function ID.** The same underlying function gets a different local
number in each script that imports it, evidently assigned by binding
order, not by any fixed global scheme. `33b549ef`'s own correlation table
survives this correction (every category-to-target pairing it reported
was measured within one instance's own trace and remains correct *for
that instance*); only the "category is THE id" framing was too strong.

**`1e90f723`'s conclusion does not survive.** "Title's script is never
bound to the completion function at all" is false — the structural dump
data in that report was accurate (nine entries, all real), but interpreting
"no entry with `category==1`" as "no `EndMode` binding" assumed category
numbers were comparable across instances. They aren't. Title's table
*does* bind `EndMode`, at its own local index 3. Same failure shape this
campaign has now hit three times (`3c7e7291`, `f7c4e68f`,
`1e90f723`): a correct, specific measurement, followed by one interpretive
step stronger than the data supports.

## What still stands, and on what evidence

Title's evaluated tick-3001 statements are `{4, 5, 6, 0xB, 0x13, 0x17}` —
`GetCurrentLevel`, `GetCurrentMission`, `GetCurrentMode`, a not-found
lookup, `OnVoice2D`, `SendMsgI`. **Category 3 (`EndMode`) is not among
them.** The load-bearing evidence for "title's script never calls
`EndMode`" is the **unwindowed** native-call trace this campaign has run
repeatedly at up to 8000 ticks (`c73498cb`, `6fc7b184`, this session's own
reruns) — `target=0x820EA4A8` fires exactly once in every one of those
runs, always at tick 2425, always startup's context. That trace is not
tick-windowed the way the lookup-key instrument is, so the earlier gap
(ticks 2460-2990 uncovered by any lookup-line window) does not weaken this
specific claim: a successful category-3 evaluation for title would
necessarily have produced a marshaller call the unwindowed trace would
have caught, at any tick, and none ever appears. **The corrected finding:
`EndMode` is bound and callable for title's script — the binding exists —
but nothing in title's evaluated statement list, across every window this
campaign has traced, ever looks it up.** This is closer to `642f77a4`'s
original "unreached branch" framing than to "absent from the script" —
option (b), not (a), contrary to `1e90f723`'s stated conclusion.

**One inference, not yet fact**: if registration is import-driven (the
loader binds whatever the script's own import list names — the natural
reading of "why would a symbol be registered at all," not yet verified),
title's `EndMode` binding implies at least one `EndMode` call site exists
somewhere in title's compiled script data, in a branch this input never
reaches. That would be the strongest form of "present but unreached" this
campaign has established — but it depends on an unproven assumption
about what drives registration, so it is stated here as a hypothesis, not
a conclusion.

## Category 11 sharpens, not resolves

Title's tick-3001 batch *looks up* local symbol `11` — the lookup fires,
the script clearly references something at that index — but `11` is not a
registered entry in title's table (`node=0, type_tag=0`, both before and
after this correction, unaffected by it). A script referencing a
never-bound local index is itself an anomaly worth carrying forward: either
that particular import failed or was conditionally skipped during
loading, or index 11 denotes something other than a native-function import
in this script's own numbering. The registration site (next) is the
discriminator for this too, not just for `EndMode`.

## The heartbeat "anomaly" closes, confirmed by direct read, not inference

`1e90f723` left open why a different context's `type_tag=2` node never
produces a marshaller call. Both contexts' vtables are static image data;
read directly:

```
title/startup class vtable (0x82007974) slot 44 (+176) = 0x820E8F90   (the marshaller)
heartbeat class     vtable (0x820074CC) slot 44 (+176) = 0x820E89B8   (a different function)
```

**Confirmed, not hypothesized: the heartbeat-driving context is a
different class, whose own slot 44 is a different native-call dispatcher
entirely.** `type_tag=2` means "this is a native-call node" uniformly, but
*which* function actually executes the call is resolved through the
calling object's own vtable, which differs by class. No anomaly — ordinary
polymorphism, invisible to instruments gated on `sub_820E8F90`
specifically because the heartbeat class never routes through it.

## Full named vocabulary, this run

**Title (`0x2E3EAA94`, class vtable `0x82007974`)**: `EndMode`(3, I→V),
`GetCurrentLevel`(4, V→I), `GetCurrentMission`(5, V→I),
`GetCurrentMode`(6, V→I), `OnVoice2D`(0x13, S→I),
`SendMsgI`(0x17, S→I), `SendMsgV`(0x18, S→V, never observed called),
`SetBuffer`(0x19, II→V), `StopBGMFadeOut`(0x1A, II→V, never observed
called).

**Startup (`0x2E3CA994`, same class vtable)**: `EndMode`(1),
`GetBGMTrackNo`(2, I→I), `PlayBGM`(4, III→V).

Retiring informal names used earlier in this campaign now that the real
ones are read from the image: "the completion trigger" is `EndMode`
(argtype `I` — startup's own call passed `0`, per `6e8fab2f`'s trace);
"NUD_TONE_BANK" (a debug-log guess from the argument string's own prefix)
is the real API function `OnVoice2D`.

## Not established

- Whether registration is genuinely import-driven — the inference above,
  unverified.
- What conditions, if any, gate whether a script's own `EndMode` call site
  (if one exists in title's data) gets reached — the registration site is
  the next concrete read, not yet done.
- Why local symbol `11` is referenced but unregistered in title's table —
  sharpened, not resolved, by this report.
- `SendMsgV`/`StopBGMFadeOut`'s bodies or purpose beyond their command-table
  signature — named, not traced.

## Gates

Source change, this commit: `dump_swg_symbol_table` gains `value0`,
`value8`, `value12` and the caller's `context_vtable` fields (all
read-only); the dump gate is decoupled from the lookup-line tick window
(it was the tick-windowed gate that produced `1e90f723`'s uncontrolled
dump in the first place) and now fires on every qualifying call, capped
at the same four-distinct-context limit. Still no new line in
`AC6_PPC_CALL_INDIRECT`. The command-table decode itself is a throwaway
static Python script reading `.build/Default.xex.base.bin` directly, not
committed — results transcribed above. Native gate JF, demo `ctest`
(26/26), and both contract audits verified below before commit.
