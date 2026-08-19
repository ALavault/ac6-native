# The loading script never looks up `EndMode`; `SendMsgI("M150")` is an
# unconditional broadcast, not a polled check

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Four live probe runs on the forced-`menu_endMode=1`
route (fresh neutral store each): a box-call/native-call correlation run
(4400 ticks), an address-range bracket run over the resulting heap arena
(4400 ticks), two symbol-table dump runs (4400 and 8000 ticks, the second
adding a new min-tick gate). Plus one direct byte-level read of
`.build/Default.xex.base.bin`'s command table. Source change: one new
env-gated parameter on the existing `dump_swg_symbol_table` call site
(`recompilation/ace-combat-6-demo/src/guest_bridge/swg_native_call_trace.hpp`).

## What this closes

`7bc566dd` eliminated both native-code candidates for "what consumes
`SendMsgI('M150')`'s answer" and named the script's own bytecode at context
`0x2E3FA914` as the last remaining avenue. This report reads that context's
own compiled vocabulary directly and answers the question: **nothing
consumes the answer because the script's own control flow never reads it
back — `SendMsgI` is sent unconditionally, every tick, forever, and the
`EndMode` binding this script also holds is never once looked up.**

## Locating the sending interpreter and its arena

`AC6_DEMO_WATCH_SWG_BOX_CALL` combined with `AC6_DEMO_WATCH_SWG_NATIVE_CALL`
over 4400 ticks: the interpreter instance `r31=0x2E3EFA08` starts calling
`box()` at **tick 4259** — the identical tick `M150`'s native calls begin —
and continues through the run's end (`tick=4399`, still firing). Its own
`box()`-time context field (`[r31+12]`, read as box's own `r3`) is constant
at `0x2E3F3D14`, in the same heap arena as `M150`'s own native-call context
(`0x2E3FA914`) and the later-dumped symbol-table context (also
`0x2E3FA914`) — all three addresses distinct, all three in the same
tick-4255-allocated arena. **This temporal/spatial correlation is not a
byte-level proof the box-call buffer and the symbol-table context are the
identical object** (see "Not established"); the symbol-table result below
does not depend on it and is independently conclusive.

Bracketing `[0x2E1F1000, 0x2E1F2000)` with `AC6_DEMO_WATCH_ADDR_LO/HI` (the
range spanning this interpreter's observed `pc_after` values) over the same
4400-tick run: every one of the 4096 bytes in that range is written exactly
once by `sub_82278F78` (the established bit-unpacking bytecode loader), all
at **tick 4255** — one tick before the mode switches to loading and `M150`
begins. (`sub_823273E0` also fills the same range once, at tick 39, with
`0xFEFEFEFE` — ordinary pool-poison allocator init, same pattern `99ec1791`
already found, not bytecode.) The buffer is reconstructed in full
(`reconstructed.bin`, not committed — throwaway analysis artifact) and
confirms this arena is a complete, single-shot bytecode fill, consistent
with a per-script buffer, though its exact statement grammar was not fully
decoded (superseded by the stronger method below).

## The symbol table is the direct answer, and needed one small instrument fix

`AC6_DEMO_DUMP_SWG_SYMBOL_TABLE` already exists (`AC6_DEMO_CORRECTING_...`),
but its capture is a fixed 4-slot, first-seen-ever cache at the AST-node
evaluator (`0x820DFFB8`) — a run long enough to reach the loading context
finds the cache already full of two earlier attract-movie context pairs
(`0x2E3C7B94`/`0x2E3CA994`, `0x2E3E7C94`/`0x2E3EAA94`). Added
`AC6_DEMO_SYMBOL_TABLE_MIN_TICK` (default 0, reproducing the original
first-4-ever behaviour exactly when unset) to bias the cache toward a later
window. With `AC6_DEMO_SYMBOL_TABLE_MIN_TICK=4200`, the dump captures
`0x2E3FA914` directly — **42 symbol-table entries**, six with small integer
categories (native-call bindings) and thirty-six with the `category=-1`
wildcard (local-variable slots, sequentially indexed by `value12`).

Decoding the six native-call bindings with `AC6_DEMO_CORRECTING_...`'s
established formula (`table_base=0x82386408`, `row = table_base +
value12*16`), then independently verifying two of them by reading
`.build/Default.xex.base.bin` directly rather than trusting the prior
report's transcription:

```
category=0x02 value12=0x22 -> row 0x82386628 -> w0=0x82007AA8="EndMode"   w3=0x820EA4A8
category=0x13 value12=0x07 -> row 0x82386478 -> w0=0x82007C20="SendMsgI" w3=0x820E9838
```

Both direct reads confirm the prior report's table exactly (`w0` is a
name-string pointer, matching the symbol-table entry's own `value0` field
byte-for-byte: `0x82007AA8`/`0x82007C20`) and both `w3` native pointers
match this campaign's own long-established addresses for `EndMode` and
`SendMsgI`. **The loading context's own script is bound to `EndMode` at its
own local index 2** — a third distinct local numbering for the same
function (startup: 1, title: 3, loading: 2), continuing `AC6_DEMO_
CORRECTING_...`'s finding that these indices are per-instance, not global.
Local index `0x13` resolves to `SendMsgI`, row `0x82386478` — the exact
`table_row` this campaign has watched on every live `M150` native call
since `69e3435f`.

## Live trace: `SendMsgI` is looked up and called every tick; `EndMode` never once

`AC6_DEMO_WATCH_SWG_LOOKUP_KEY` over an 8000-tick run (the instrument's own
tick window is `[2990,8000]`, covering the entire observed `M150` history
and roughly 2600 further ticks of steady state): **12465 lines**, all from
the single caller site `lr=0x820D452C`. From tick ~4400 onward the pattern
is invariant, sampled at the run's tail:

```
tick=7997  category=0x13 node=0x2E3FB610 type_tag=2   (matches the dumped
                                                         SendMsgI entry's
                                                         own node address,
                                                         exactly)
tick=7997  AC6_SWG_NATIVE_CALL ... context=0x2E3FA914 table_row=0x82386478
                                    tag=M150            (immediately after)
tick=7997  category=0x28 node=0x2E3F85D0 type_tag=2   (see "Not established")
```

Repeating unconditionally, once per tick, for the full 3600-tick sampled
tail. **`category=0x00000002` — the loading script's own local index for
`EndMode` — appears zero times anywhere in the 5010-tick window.** Grepped
for the exact token, not inferred: `grep -c "category=0x00000002 "` returns
`0`.

## Conclusion

The mechanism this campaign proved live in `69e3435f` (`SendMsgI("M150")`
genuinely returns `1`, correctly, at tick 5413) was never broken and was
never going to be consumed, because **the calling script's own compiled
logic does not read the return value back into any branch at all.** The
per-tick evaluation is unconditional: the same two local symbols get looked
up and (for `0x13`) called every single tick, regardless of what the
previous call returned. `EndMode` — bound, callable, verified present in
this exact context's own table — is simply never referenced by anything
this script's evaluator does, in any of the 5010 ticks measured. This is
not a blocked poll; it is a script that broadcasts `M150` as a pure,
unconditional heartbeat and was never written (or never reaches, in any
window this campaign can drive) a follow-up action on its own answer.
Combined with `fbd10eef`'s finding that every registered listener is
already dead at the moment of the call, the full picture closes: `M150`'s
answer has no reader on either side — no listener alive to receive it, and
no branch in the sender's own script that ever asks.

This is the third time this campaign has found the identical shape (bound,
correct, never evaluated): `AC6_DEMO_CORRECTING_CATEGORY_1...` for title's
own `EndMode`, `883d396d` for message 102, and now this, for message 150 —
strong enough repetition to treat "compiled-but-unreached script branch" as
this demo's dominant failure mode for the black-frame investigation, not a
one-off.

## Consequence for the plan

The M150/loading-task thread is now closed end-to-end: mechanism proven
correct, consumer proven absent, and the absence itself proven structural
(a script property, not a native-code bug) rather than merely unobserved.
Advancing past the black frame is not a matter of fixing `SendMsgI`,
`CModeTaskLoadingDemoOffline`'s gates, or any native dispatch code already
traced this campaign — all of it works as designed. What remains open is
whether *some other* script, mode, or input (not the forced-`menu_endMode`
route this whole thread has driven) ever does evaluate local index 2 in a
context holding this same `EndMode` binding, or whether a completely
different control path (outside the `swg` scripting layer entirely) is
meant to advance the demo past loading. Per `plan mode`'s Phase 1/2 framing,
this argues for returning to the import-diff/oracle-comparison method next,
rather than continuing to narrow within the `swg` layer — three closed
threads inside the same subsystem is a signal to change subsystem, not to
open a fourth.

## Not established

- Whether the `0x2E1F1000-0x2E1F2000` bytecode arena (fully reconstructed,
  4096/4096 bytes, single fill at tick 4255) is byte-for-byte the same
  script object as symbol-table context `0x2E3FA914` — correlated by tick
  and address neighborhood, not proven by a direct pointer chase. The
  symbol-table/lookup-key result above does not depend on this and stands
  on its own.
- What `category=0x28`/node `0x2E3F85D0` is — evaluated every tick
  alongside `0x13` from the same `lr`, but absent from context
  `0x2E3FA914`'s own 42-entry dump, so it likely belongs to a second,
  uncaptured context interleaved in the same trace (the lookup-key
  instrument does not log which context originates each line). Not
  identified.
- What local index `0x14`'s row (`0x82386418`, one slot after `SendMsgV`'s
  own row `0x82386408`) resolves to — not decoded, not load-bearing for
  this report's conclusion.
- Whether any route other than the forced-`menu_endMode` path this thread
  has exclusively used would ever drive a script to evaluate local index 2
  in this or an equivalent context.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. One source change: `AC6_DEMO_SYMBOL_TABLE_MIN_TICK`,
opt-in, defaults to 0 which reproduces `AC6_DEMO_DUMP_SWG_SYMBOL_TABLE`'s
prior behaviour exactly (verified: the unset-env first probe run in this
report's own investigation, before the fix, reproduced the original
four-context set). Default route behavior unchanged.
