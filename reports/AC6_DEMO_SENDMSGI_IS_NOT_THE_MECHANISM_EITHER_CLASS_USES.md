# The message-listener path is ruled out by a working control; the real transition lives inside `0x820CE368`'s callees

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence: `.build/Default.xex.base.bin`
(RTTI walks, direct vtable reads) and `codegen/generated/ppc_recomp.{15,16}.cpp`
(control-flow evidence only). No oracle.

## Correction to my own first draft

This report originally concluded "the title's message-listener vtable has
no implemented handler but the destructor, so any message is silently
dropped." That draft mis-scoped the vtable's extent (missed a real,
non-stub slot at `+0x54`, wrongly read a padding word at `+0x58` as
evidence the vtable ended at `+0x50`) and, more importantly, never checked
which slot the actual dispatcher calls before concluding "no path exists."
Caught before commit. The corrected work below supersedes it.

## What actually gates dispatch, and why the listener path is dead — but not for the reason first claimed

`trace_message_listeners`'s own comment (`frontend_state_trace.hpp:111-112`,
established earlier this campaign) already says it: `SendMsgI` broadcasts
to the listener array at `0x826DF800` and calls **slot `+0x20`** on each
listener. Dumped in full, the title's own listener vtable (`0x82011384`,
`CModeTaskTitleDemoOffline`, registered at tick 2452) is 22 slots
(`+0x00`..`+0x54`, followed by 4 bytes of padding then the next vtable's
RTTI locator — not 21 slots as first read): slot 0 is a real function
(`sub_82190820`, one of 24 adjustor thunks in `sub_821907F8` — `this -= 104;
tail-jump` — resolving to `sub_8218F2F8`, a classic MSVC vector-deleting
destructor, not a message handler); slots `+0x04`..`+0x50` are the
confirmed shared no-op (`0x820AC748`); slot `+0x54` **is** a real function
(`0x8217C890`, in the `CModeTaskGame*`-family address neighborhood
`ce065acb` already named). Slot `+0x20` — the one `SendMsgI` actually
calls — is a stub for this listener.

**Control test, before trusting that as the explanation:** the startup
task's own listener vtable (`0x820112AC`, `CModeTaskStartUpDemoOffline`)
has the *identical* 22-slot layout, and its slot `+0x20` is **also** the
same stub. Startup's state machine demonstrably advanced 0→1→2 anyway
(`AC6_MODE_STATE`: state 1 at tick 266, state 2 at tick 2426). If a stub at
`+0x20` were sufficient to explain title being stuck, startup should be
stuck too — it isn't. **`SendMsgI`/slot `+0x20` is not the mechanism that
advances either class's state**, and the entire message-listener framing
this report started with is the wrong thread.

## Where the real transition has to be, established by elimination

Both classes' `update` functions (`sub_8218A4A0` startup, `sub_8218A7A8`
title) have the identical 4-state shape: state 0 checks a readiness
condition and writes `[this+12]=1` directly if met (confirmed present in
startup's code, and empirically title does the same — both observed to
reach state 1); state 1 calls exactly one thing — `[this+28].vtable+0x20`
— and returns, no other logic; state 2 is a countdown then
`manager->request=1`; state ≥3 is idle. Neither state 1 handler writes its
own object's state.

Startup's own `this+28` sub-object is confirmed (`AC6_SWG tick=222
sub=0x2E7E009C vptr=0x82006438`) to be the **same `CSwgManager` vtable**
title uses, so state 1's `+0x20` call resolves to the **same shared
`sub_820CE368`** for both classes. `fcecb736` already read that function's
entire body: exactly six stores, none touching the caller's state field.
That was checked and is correct — but it only rules out `0x820CE368`
*itself* writing state. Since startup's state 1→2 transition is real and
observed, and nothing else in the reachable call chain writes it, **the
write has to happen inside one of `0x820CE368`'s own callees** — most
plausibly inside the `CSwgCallback+9`-gated block (the one substantial,
conditionally-skipped piece of this function, which itself makes four
further indirect calls, including the `player.vtable+68` "draw frame N"
call and two calls through `sub_82327D90`-style helpers not traced).

This *reinforces* rather than undercuts the standing diagnosis: for
startup's boot-logo `CSwgCallback` instance, something evidently does pulse
its `+9` flag at some point (its state machine completed); for title's own
instance, `956bd743` already proved exhaustively that nothing reachable
ever does. The mechanisms converge — completion is signaled from inside
the flag-gated block — and title's copy of that block never runs, for the
already-established reason.

## Conclusion

Retracts: "message-listener interface is the reason title is stuck."
Stands, more precisely stated: `SendMsgI`'s broadcast path is dead for
*both* classes and is not the mechanism at all; the actual state-1→2
signal lives somewhere inside `0x820CE368`'s callees (most likely the
flag-gated frame-step block itself, via one of its own virtual calls), and
tracing exactly which callee performs that write — using startup's working
instance as a live example — is the concrete next step, not yet done.

## Not established

- Which specific callee of `0x820CE368` (inside the flag-gated block) is
  the one that would write `[title_this+12]` — not traced.
- Whether startup's own `CSwgCallback` instance was ever observed live with
  its `+9` flag actually set, and by what — not checked; would confirm the
  "the flag does get pulsed for startup" half of this report's inference
  rather than leaving it as elimination-based.
- What `0x8217C890` (title's own real slot `+0x54`) and `0x821728C0`
  (startup's) do, and whether either is reachable — not read.

## Gates

No source changed; report-only commit.
