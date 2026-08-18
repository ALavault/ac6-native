# Title's five post-START native calls, all named: mode/level/mission queries, a dead broadcast, an audio log — never the completion command

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence:
`codegen/generated/ppc_recomp.6.cpp` (control-flow evidence only). Live
evidence: a targeted instrument (`AC6_DEMO_WATCH_SWG_NATIVE_CALL`,
`guest_bridge.cpp`, gated on the exact `bctrl` return address inside
`sub_820E8F90`), built up over three runs on the same store/route
(correctly-timed START at tick 3000, headless backend, no oracle):
`probe --max-ticks 3600` for the initial 9-call capture, `--max-ticks 8000`
(5000 ticks post-START) to check for a delayed reaction, and a second
`--max-ticks 3600` pass after extending the instrument to dereference the
`SendMsgI` message argument.

## What this closes

`642f77a4` traced the state-advance chain back to `sub_820E8F90`, the
ActionScript native-call marshaller, reached 9 times in the whole
12000-tick run, and left "what native command does each of the 9 calls
issue" as the concrete next step. This instruments it directly.

## The instrument

`sub_820E8F90`'s dispatch is `mtctr r10=[r23+12]; bctrl` at a fixed return
address, `0x820E9130`, where `r23` (unreassigned since function entry)
holds the resolved command-table row: `table_base + command_index*16`.
Added `trace_swg_native_call` (`guest_bridge/swg_native_call_trace.hpp`,
called from `AC6_PPC_CALL_INDIRECT` in `guest_bridge.cpp`, same
extract-to-header pattern as the existing `trace_dynamic_object_vtable`)
that, behind `AC6_DEMO_WATCH_SWG_NATIVE_CALL=1`, logs `tick`, `target`
(`guest_address`,
i.e. `ctx.ctr`), `table_row` (`ctx.r23`), and the owning `context`
(`ctx.r31`) whenever `lr == 0x820E9130`. Extended once more to log
`arg_count` (`ctx.r28`), the marshaled-args pointer `args` (`ctx.r26`,
confirmed as `sub_820E8F90`'s own `r6` argument to the target by reading
its dispatch setup), and `first_arg` — the first 32-bit word at `args`.
When the target is `sub_820E9838` (`SendMsgI`) and `first_arg` is nonzero,
also dereferences it as a 4-byte tag (`tag=`), since reading
`sub_820E9838`'s body showed it passes `first_arg` unchanged to
`sub_820E9388`, which treats it as a pointer and validates it as a 4-byte
tag before ever reaching a listener.

## The measurement

Nine calls total in the 3600-tick run, exactly matching the atlas's count.
**Re-run at `--max-ticks 8000`** (5000 ticks after the tick-3000 press, over
five times the original post-press window) **produced the identical nine
calls and no tenth** — the "title issues exactly these five and never the
completion command" claim below is not an artifact of a short sampling
window; the script does not react again, late, within this window either.
Final capture, with argument decoding:

```
tick=1045  target=0x820EA298  table_row=0x823865C8  context=0x2E3CA994 (startup)   arg_count=1  first_arg=0x00000001
tick=1045  target=0x820EA128  table_row=0x82386588  context=0x2E3CA994 (startup)   arg_count=3  first_arg=0x0000001D
tick=2425  target=0x820EA4A8  table_row=0x82386628  context=0x2E3CA994 (startup)   arg_count=1  first_arg=0x00000000  <- the completion trigger, 642f77a4
tick=2452  target=0x820EA0A8  table_row=0x82386548  context=0x2E3EAA94 (title, first call after mode switch)  arg_count=2  first_arg=0x00000004
tick=3001  target=0x820EA598  table_row=0x82386658  context=0x2E3EAA94 (title, right after START)  arg_count=0
tick=3001  target=0x820EA550  table_row=0x82386648  context=0x2E3EAA94  arg_count=0
tick=3001  target=0x820EA538  table_row=0x82386638  context=0x2E3EAA94  arg_count=0
tick=3001  target=0x820E9838  table_row=0x82386478  context=0x2E3EAA94  arg_count=1  first_arg=0x2E403110  tag="M102"
tick=3001  target=0x820EA6C0  table_row=0x82386568  context=0x2E3EAA94  arg_count=1  first_arg=0x2E4032D0
```

The five at tick 3001 are title's direct, immediate script response to the
press — matching `eab92d66`'s "the press derails the ActionScript film into
other script handlers" observation exactly, now with every handler named.

## The five, identified

- **`sub_820EA538` = `GetCurrentMode`**: reads `[gs+120]` directly (`gs` =
  `[0x823C27E0]`, the singleton `3c7e7291` already used).
- **`sub_820EA550` = `GetCurrentMission`**: calls `sub_820E9300` on
  `[gs+112]` (the exact validity-gate function `3c7e7291` read) and falls
  back to `16` when it fails — the same fallback value `3c7e7291` already
  measured live.
- **`sub_820EA598` = `GetCurrentLevel`**: calls `sub_820E9290` on
  `[gs+112]` (the sibling switch `3c7e7291` also read, "no case matches")
  and remaps its result (`6↔7`) before returning it.
- **`sub_820E9838` = `SendMsgI`**: passes its `first_arg` pointer unchanged
  to `sub_820E9388`, which reads it as a 4-byte tag and validates it
  (byte0 must be `0x4D`='M', bytes 1-3 nonzero, else the call returns `-1`
  and no listener is ever reached). Title's live call carries
  `tag="M102"` — a well-formed tag that **passes** this check. Having
  passed, `SendMsgI` reads the listener array at `0x826DF800` and calls
  **slot `+0x20`** on each populated entry — the exact slot `69ff833a`
  already proved is a shared no-op stub for every listener checked (both
  startup's and title's). **This is not a malformed or rejected call: a
  real, validated message is sent and has nowhere to land.**
- **`sub_820EA6C0` = a `NUD_TONE_BANK_` audio log/debug utility**: checks
  whether its string argument starts with `"NUD_TONE_BANK_"`, formats it
  (`"NUD_TONE_BANK_%s"` or plain `"%s"`), and calls a virtual print/log
  method. Depends on `sub_820CE368` having mirrored `[CSwgManager+32]`
  into a fixed global first — unconditional, unrelated to the dead flag.
  Audio-adjacent, not state-related.

**All three of `eab92d66`'s originally-named handler categories
(`GetCurrentMode/Mission/Level`, `SendMsgI`, and an audio-adjacent
handler) are now resolved to exact addresses and bodies.** None of the
five is `sub_820EA4A8` (`table_row=0x82386628`), the one startup's script
called at tick 2425 to trigger the completion chain (`642f77a4`).

## Conclusion

Title's script, in the 5000 ticks after a correctly-timed START (measured
to the edge of the sampling window, not just its first 600), asks three
already-known-broken questions (mode/mission/level, all keyed on the same
`[sub112+8]`/`[gs+112]` field `3c7e7291` proved constant regardless of
START), sends a well-formed, validated message (`SendMsgI`, `tag="M102"`)
through an already-known-dead channel (slot `+0x20`), and logs an
audio-bank string — and does none of the one thing that would advance its
own state. Two candidate faults were considered:

1. **Upstream fault** (`642f77a4`'s standing hypothesis): the script
   branches on the mode/mission/level results, and because those results
   are wrong (forced fallback values, not what a correctly-initialized
   script would see), it takes a "nothing to do" branch that never issues
   a real signal.
2. **Downstream fault**: the script's decision is correct and it does
   signal — but the only channel it has (the listener array at
   `0x826DF800`, slot `+0x20`) is a confirmed no-op for every listener
   checked.

**The `tag="M102"` measurement favors (2).** `sub_820E9388` rejects a
malformed tag before any listener runs (`byte0 != 'M'` or any of bytes 1-3
zero → return `-1`, no listener call at all) — that is exactly the shape a
"nothing to do" branch would produce if it called `SendMsgI` at all: it
would either not call it, or call it with a zero/garbage argument that
fails validation. Instead title's script assembles a specific,
well-formed, byte-by-byte-valid 4-character tag and pushes it all the way
through to the listener loop. That is the behavior of a script that
*believes* it is sending a real, meaningful signal — not one taking an
empty branch. The listener side is where the signal is lost: slot `+0x20`
is confirmed dead for every listener on both classes' arrays (`69ff833a`).
This does not fully retire candidate (1) — a wrong mode/mission/level
answer could still be why the script sends `"M102"` instead of whatever
tag the completion path would send — but it does establish that **this
particular call is not evidence of an empty/no-op branch; it is evidence
of a real signal dying in a dead channel.**

## Not established

- The exact branch logic inside the script bytecode that decides, from
  these five query/broadcast results, not to call the completion command —
  this report identifies the *native* handlers the script calls, not the
  *script* logic driving which ones get called or what it does with their
  results. That is the ActionScript bytecode itself, still unlocated
  (per the standing "compiled clip data's location remains unfound" note).
- Whether feeding the script correct (non-fallback) mode/mission/level
  values would change its behavior — not testable without either finding
  the seed writer for `[sub112+8]` (exhaustively shown unreached,
  `8fba5b45`-adjacent work) or the script bytecode itself.
- What `sub_820EA298`/`sub_820EA128` (startup's own two preliminary calls,
  tick 1045) do, and what their own `first_arg` values (`0x1`, `0x1D`)
  mean — not read.
- What `"M102"` specifically denotes (a UI transition code, a mission
  select event, something else) — not identified. It is not present as a
  static string literal anywhere in the XEX image (checked); `sub_820E9388`
  builds it byte-by-byte from register/stack values rather than loading a
  string constant, so its meaning would have to come from the script
  bytecode that supplies those bytes, or from a table of recognized tags on
  the receiving side — neither located.
- Whether a listener that *did* implement slot `+0x20` for `"M102"` would
  in turn call the completion path (`sub_820EA4A8`'s `+0x54` mechanism, or
  something else) — not traced; slot `+0x20`'s only confirmed behavior
  anywhere in this campaign is "absent/no-op," so what a real
  implementation would have done is not established.

## Gates

`AC6_DEMO_WATCH_SWG_NATIVE_CALL` in
`guest_bridge/swg_native_call_trace.hpp` (extracted from
`AC6_PPC_CALL_INDIRECT` into its own header, same pattern as
`trace_dynamic_object_vtable`, after the inline version pushed that
function to 221 lines against a 220-line budget) is a new, opt-in-only
addition — one env var, unset by default; one `if` check per indirect
call, immediate no-op when unset. Native gate JF: pass. Demo ctest: 26/26.
Both contract audits: pass (`contract_artifacts=pass contracts=6 cited=189
match_head=189`; `contract_addresses=pass contracts=6 cited=321
supported=321 unsupported=0`).
