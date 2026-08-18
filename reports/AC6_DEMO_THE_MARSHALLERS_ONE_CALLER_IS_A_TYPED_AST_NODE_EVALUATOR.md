# The marshaller's one caller is a typed AST-node evaluator, not a bytecode loop directly

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `probe --until frontend
--max-ticks 4000`, new read-only instrument `AC6_DEMO_WATCH_SWG_VM_LOOP_CALL`
(`guest_bridge/swg_native_call_trace.hpp`), headless backend, no oracle,
correctly-timed START. Static: `sub_820DFFB8`'s full generated body
(`ppc_recomp.5.cpp:28553-28667`).

## What this closes

Memory (`642f77a4`'s correction) stood since earlier this campaign:
`sub_820E8F90` (the native-call marshaller) "is reached from the swg script
interpreter through some other, uninstrumented call path (an indirect
`bctrl`, like everything else in this subsystem)" — the caller itself was
never named. This was the advisor-suggested next move: every indirect call
already passes through `AC6_PPC_CALL_INDIRECT`, so naming the caller needed
no new discovery mechanism, only a symmetric instrument to the one that
watches calls *out of* the marshaller.

## The instrument

`trace_swg_native_call` (already hooked into every `AC6_PPC_CALL_INDIRECT`
resolution) gained an independent, separately-gated block:
`AC6_DEMO_WATCH_SWG_VM_LOOP_CALL`, firing whenever
`guest_address == 0x820E8F90U` (any call *targeting* the marshaller,
regardless of `lr`), logging `lr`, `r1`, `r3`, `r31`.

## The result

All 9 calls to the marshaller, across the whole run, share **one exact
return address**: `lr=0x820E0080`. `r1` (stack pointer) is also identical
across every call (`0x7F040358`) — the same physical call site, not merely
the same function reached via different stack depths. `r31` alternates
between exactly the two values `AC6_SWG_NATIVE_CALL`'s own `context=` field
already showed (`0x2E3CA994` for startup, `0x2E3EAA94` for title) —
independent cross-confirmation that `r31` at this call site is the same
per-VM-instance context object already tracked.

`lr=0x820E0080` sits inside `sub_820DFFB8` (`ppc_recomp.5.cpp:28553`,
starts at `0x820DFF98`ish — the `bctrl` whose next instruction is
`0x820E0080` is the function's only indirect call). This confirms and
extends `642f77a4`'s note: `sub_820E8F90` has no direct `bl` caller because
its one caller reaches it through a virtual dispatch, not because it's
reached from many places — it's reached from exactly one code location,
indirectly.

## `sub_820DFFB8`, read in full

Signature (from register moves at entry): `(r3=result_ptr, r4=context,
r5..r8=four more args)`. Saves `r26=result_ptr`, `r31=context`,
`r30=context+36`, `r29/r28/r27` = the three extra args.

1. Calls `sub_820DFD28(&local[r1+80], context+36, arg5, ...)` — a lookup:
   writes two output words, `[r1+80]` and `[r1+84]`, from inputs including
   `context+36` (`r30`, saved separately — the STRUCT THIS FUNCTION treats
   as a lookup root) and the trailing args threaded through unchanged.
2. Sanity-checks the lookup result with two `twi 31,r0,22` traps (trigger
   if `[r1+80]==0` or `[r1+80]==context+36` itself) — defensive asserts on
   a successful, non-self-referential lookup.
3. Reads `[r1+84]` (`r10`) and compares it against `[[r1+80]+4]` (`r9`);
   equal → skip straight to a "zero result, return" path
   (`loc_820E0084`); not equal → continue.
4. A second lookup-shape check (`r9=[r11+4]` where `r11` is the same
   `[r1+80]`) against `r10` again, another `twi` on mismatch.
5. **The type check that gates the native call**: `r9 = [[r10+28]+4]`;
   `cmpwi r9,2`; not-2 → same zero-result skip path. **Only when this field
   reads exactly `2`** does the function proceed to the virtual call.
6. The call itself: `r11 = [r31+0]` (context's own vtable), `r11 =
   [r11+176]` (**slot 44**, `176/4`), `mtctr r11; bctrl` — args
   `r3=result_ptr(r26), r4=context(r31), r5=2 (literal), r6=[r10+28]`.
   `sub_820E8F90` occupies slot 44 of the context object's own vtable in
   every observed call, in this build.
7. On the skip path: `[result_ptr+0] = 0` (a real, distinguishable "no
   value" write — matches the marshaller's own zero-initialized "handled"
   pattern this campaign already traced downstream of).

## Reading

This is not the interpreter's dispatch loop itself — it's a per-node
**typed evaluator**, almost certainly called once per AST/bytecode node
during a script's execution, that special-cases exactly one node kind
(`type==2`, read from a field at a fixed offset off a resolved symbol-table
entry) as "this node is a native-function call" and routes it through the
context's own vtable slot 44 — which is `sub_820E8F90` for every context
observed, but the indirection through a vtable slot (not a hardcoded
address) means other node types plausibly route through other slots of the
same vtable, unread here. `context+36` is the natural next read: it's what
`sub_820DFD28` treats as a lookup root, and is the closest thing to "the
symbol table" or "the current AST/bytecode node" this campaign has found
with a live, exercised access path (9 hits, not 0).

## Not established

- What `sub_820DFD28` actually does — read only by its call shape and
  outputs, not its own body.
- What `context+36` structurally is (a symbol table, a node list, a single
  current-node pointer) — inferred from role, not read directly.
- What the other 43 (or however many exist) vtable slots on the context
  object route to, or whether any of them is the actual bytecode
  program-counter advance / dispatch-loop step.
- Whether `type==2` is one of several typed branches this function has (it
  reads as exactly one `cmpwi`, so likely yes — a real dispatch would
  handle more types, either here or in a sibling function not yet found).
- Whether this function is itself called in a loop (the interpreter's
  per-instruction step) or once per script "statement" from a different
  driving loop — its own callers are not found (same indirect-call pattern
  as its own callee).

## Gates

New env var `AC6_DEMO_WATCH_SWG_VM_LOOP_CALL`, opt-in, read-only, unset by
default. Folded into the existing `trace_swg_native_call` function (no new
call site or line added to `AC6_PPC_CALL_INDIRECT` itself, which remains at
its 220-line complexity budget). Native gate JF, demo `ctest` (26/26), and
both contract audits verified below before commit.
