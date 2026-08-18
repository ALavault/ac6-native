# Every listener actually registered when title sends `"M102"` shares the dead `+0x20` slot

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence: RTTI walk against
`.build/Default.xex.base.bin` (locator/type-descriptor read, same manual
procedure this campaign has used throughout — `analysis/class-map.tsv`/
`tools/whose_vtable.py` do not apply to the demo build) and
`codegen/generated/ppc_recomp.{4,10}.cpp` (control-flow evidence only).
Live evidence: `AC6_DEMO_WATCH_SWG_LISTENER_ARRAY`, a new instrument in
`guest_bridge/swg_native_call_trace.hpp`, `probe --until frontend
--max-ticks 3600`, correctly-timed START at tick 3000. No oracle.

## What this closes

`AC6_DEMO_TITLES_FIVE_POST_START_NATIVE_CALLS_NAMED_NONE_IS_COMPLETION.md`
found that title's `SendMsgI` call carries a well-formed, validated tag
(`"M102"`) that reaches the listener-array pump and dies there, because
slot `+0x20` is a confirmed no-op stub — but only two classes' listener
vtables had ever been checked at that slot (startup's own,
`CModeTaskStartUpDemoOffline`, and title's own, `CModeTaskTitleDemoOffline`).
The array holds up to 16 entries (`sub_820CE010` bounds the scan to
`base+64` bytes / 4); whether some *other*, unchecked registrant might
implement a real handler was open. This instruments the array directly
at the moment of the call instead of assuming only the two known classes
are ever present.

## The instrument

`sub_820CE010`'s own bound (`cmpwi cr6,r11,16` / loop while `r10 < base+64`)
confirms the array is `std::uint32_t listeners[16]` at `0x826DF800`, each
slot either 0 (empty) or a pointer to a listener object. Added a block to
`trace_swg_native_call`, gated on `AC6_DEMO_WATCH_SWG_LISTENER_ARRAY=1`,
that — only when the traced call target is `sub_820E9838` (`SendMsgI`) —
walks all 16 slots and logs each populated one's object pointer and the
32-bit word at that object's own address 0 (its vtable pointer).

## The measurement

At tick 3001 (the same tick as the `"M102"` call), exactly two slots are
populated:

```
AC6_SWG_LISTENER_SLOT tick=3001 slot=0 object=0x18BA2C08 vtable=0x8200A584
AC6_SWG_LISTENER_SLOT tick=3001 slot=1 object=0x2E3C0168 vtable=0x82011384
```

`vtable=0x82011384` is title's own already-known listener vtable
(`CModeTaskTitleDemoOffline`, `69ff833a`). `vtable=0x8200A584` is new —
RTTI-walked (`[vtable-4]` → `RTTICompleteObjectLocator` → `+0x0C` → type
descriptor → `+8` mangled name): **`.?AVCSelectMessageDlgManager@@`** —
"Select Message Dialog Manager." Its object address (`0x18BA2C08`) is far
below the heap range every other object seen this campaign occupies
(`0x2E3xxxxx`-`0x2E4xxxxx`) — consistent with, but not proof of, an
early-allocated system fixture. **This is a single tick-3001 snapshot, not
a sweep** — this instrument fires only on a marshalled `SendMsgI` call and
one such call was captured. `642f77a4` (not `fbd898c1`, corrected here)
asserted, without measuring, that "listener `[0]` is a permanent fixture";
this report is the first time that slot's occupant has actually been
identified, and only at this one tick.

`CSelectMessageDlgManager`'s vtable, dumped (24 slots read to find the
extent):

```
+0x00: 0x8212DD70   real (vector-deleting destructor shape)
+0x04: 0x820AC748   stub
+0x08: 0x820AC748   stub
+0x0C: 0x8212DD30   real, non-stub
+0x10: 0x820AC748   stub
+0x14: 0x820AC748   stub
+0x18: 0x820AC748   stub
+0x1C: 0x820AC748   stub
+0x20: 0x820AC748   stub — this is the slot SendMsgI calls
+0x24..+0x54: 0x820AC748   stub (13 slots)
+0x58: 0x4D736744   not a slot — ASCII "MsgD"
+0x5C: 0x6C675F53   not a slot — ASCII "lg_S"
```

The `+0x58`/`+0x5C` words are not vtable entries: they are the start of a
C string, `"MsgDlg_ShowWaitDlg"` (read at `0x8200A5DC`) — this *proves*
the vtable ends at `+0x54`, 22 slots, the identical extent to both task
listeners already dumped (`69ff833a`). A vtable-extent misread is exactly
what burned that report once already; reading the trailing bytes as data
here, rather than assuming the array continues, avoids repeating it.

`+0x0C` is real: `sub_8212DD30(this, r4, r5, r6)` — `if (r4 != 160) return;`
then walks a linked list off `[this+68]` comparing `[node+0]` against `r5`,
and on match stores `r6` at `[node+16]` and sets a ready flag at
`[node+12..13]`. This is a genuine message-style handler, but it is reached
through a **different** dispatch interface than the one `SendMsgI` uses
(its own selector argument, `r4==160`, is a plain integer, not the 4-byte
`'M'`-prefixed tag `sub_820E9388` validates) — whatever calls `+0x0C`
is not this campaign's `SendMsgI`/listener-array pump, which only ever
calls **`+0x20`**. `+0x20` is the shared no-op stub for this class exactly
as it is for both task classes already checked.

## Conclusion

**Every listener actually registered in the array at tick 3001 — not just
the two classes previously checked, but the complete, live population at
that instant, confirmed by walking all 16 slots — shares the same dead
`+0x20` stub.** This closes one specific escape hatch: "an unchecked third
listener might have handled it" no longer applies to this snapshot. It
does not settle which fault is responsible for the demo's stall. A slot
that is stubbed by *every* class observed to register — including
`CSelectMessageDlgManager`, which implements a real message-shaped handler
on a *different* interface (`+0x0C`, selector `160`) but not this one —
reads as consistent with "this interface is unused by design in the demo
build" as much as with "a delivery mechanism is broken." `69ff833a`'s own
control is relevant here too: startup's state advanced with the identical
dead `+0x20`, so nothing yet shows that a working `+0x20` would matter to
state progression at all, independent of whether it exists anywhere.

**The which-fault question from `...NAMED_NONE_IS_COMPLETION.md` (upstream
wrong branch vs. downstream dead channel) is therefore reopened, not
settled, by this census.** The concrete discriminator: sweep every
RTTI-locatable vtable in the image for a non-stub function at the `+0x20`
position of this specific 22-slot listener interface. Zero implementors
anywhere in the whole image would make "dead by design" the stronger
reading and shift weight back toward the upstream/script hypothesis; any
real implementor found would make "a working recipient exists but isn't
reached" the live question instead. That sweep is the named next step, not
attempted here.

## Not established

- Whether `+0x20` of this interface is implemented by *any* class anywhere
  in the demo image — the discriminator named above; not attempted in this
  report, which only walked the classes actually observed live in the
  array at one tick.
- What actually calls `CSelectMessageDlgManager`'s real `+0x0C` handler,
  and what message code `160` corresponds to — not traced; a distinct
  dispatch path from `SendMsgI`, out of scope for this report.
- Whether the array's population differs at other ticks (e.g. what was
  registered before startup's own listener left the array, or whether a
  third class transiently registers and unregisters between ticks) — not
  swept; only the tick-3001 snapshot, coincident with the `"M102"` call,
  was captured.

## Gates

`AC6_DEMO_WATCH_SWG_LISTENER_ARRAY` in `swg_native_call_trace.hpp` is a
new, opt-in-only addition (one env var, unset by default; a bounded
16-iteration scan only when both the env var is set and the traced call
target is `SendMsgI`, immediate no-op otherwise). Native gate JF: pass.
Demo ctest: 26/26 (`build`, not just `build-codegen-on`, was rebuilt —
`cmake --build build` first reported "no work to do" despite the header
edit predating it; a `touch guest_bridge.cpp` forced the recompile, worth
noting as a ninja/header-dependency gap in this build, not a real
no-op). Both contract audits: pass (`contract_artifacts=pass contracts=6
cited=189 match_head=189`; `contract_addresses=pass contracts=6 cited=321
supported=321 unsupported=0`).
