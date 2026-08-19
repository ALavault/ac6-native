# The forced route constructs a real `CModeTaskLoadingDemoOffline`, whose
# message-150 handler polls one readiness flag that never goes true

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run (`probe --until frontend
--max-ticks 5000`, fresh neutral store, `AC6_DEMO_FORCE_MENU_ENDMODE_ARG=1
AC6_DEMO_FORCE_MENU_ENDMODE_AT_TICK=4251 AC6_DEMO_WATCH_SWG_NATIVE_CALL=1
AC6_DEMO_WATCH_SWG_LISTENER_ARRAY=1`, both pre-existing instruments, the
latter added by `fbd10eef`), a manual RTTI walk against
`.build/Default.xex.base.bin` (same procedure `fbd10eef`/`883d396d` used;
`analysis/class-map.tsv` does not cover the demo build), and a static read
of `sub_8217E258` (`ppc_recomp.14.cpp:32587-32629`). No source change.

## What this extends

`994109dc` (this session) proved the forced-`menu_endMode=1` route's
steady state is permanent and named `0x820E9838`'s `M150` broadcast as
"the one mechanism still firing every tick on this route with visible
activity and no visible effect... what's registered in its table... is the
concrete next question." This report answers it, reusing an instrument
(`AC6_DEMO_WATCH_SWG_LISTENER_ARRAY`) `fbd10eef` already built for the
same table at a different tick, rather than writing anything new.

## The listener array holds exactly two entries, stable for 750+ ticks

`AC6_DEMO_WATCH_SWG_LISTENER_ARRAY` dumps all 16 slots of the array at
`0x826DF800` whenever `sub_820E9838` (`SendMsgI`) is called. Across every
one of the 741 `M150` calls this run captured (tick 4251-4999), the
populated slots are **byte-identical every single time**:

```
slot=0 object=0x18BA2C08 vtable=0x8200A584   (unchanged since fbd10eef's tick-3001 census)
slot=1 object=0x2E3C0268 vtable=0x820110F4   (new, first appears tick 4259)
```

Slot 0 is `fbd10eef`'s already-identified `CSelectMessageDlgManager` —
same object, same dead `+0x20` stub, present at every tick this campaign
has ever sampled this array. Slot 1 is new. It **first appears at tick
4259** — the exact tick `994109dc`'s gamestate trace recorded the forced
route's `AC6_MODE_INNER` cycling to `state=4`, immediately after the
tick-4255 mode switch — and stays through the rest of the run.

## Slot 1 is a genuine `CModeTaskLoadingDemoOffline`

RTTI walk on vtable `0x820110F4`: `RTTICompleteObjectLocator` at
`0x82074050`, subobject offset field (`locator+4`) = `0x68` (104 decimal —
the same `CSwgListener`-subobject displacement `883d396d` established for
every other listener vtable this campaign has read), type descriptor name
`.?AVCModeTaskLoadingDemoOffline@@`. The object address confirms it:
`0x2E3C0268 - 0x68 = 0x2E3C0200` — **the exact mode object address
`994109dc`'s `AC6_MODE_SWITCH` line recorded at tick 4255.** The forced
route does not merely switch to an opaque new mode object; that object is
a loading-task class, and it registers itself as a `SendMsgI` listener
four ticks after construction.

## Its `+0x20` handler is real, and only recognizes code 150

`883d396d` found this class's family (`CModeTaskLoadingDemoOffline` and
seven siblings) shares one non-stub handler, `sub_8217E258`, but had not
read its body. Read in full here:

```cpp
if (r4 != 150) return;                    // recognizes ONLY message 150
r11 = [this-92];                          // == [primary_object+12]
if (r11 != 1) { *out = 0; return; }       // gate 1: a state field must be 1
r11 = [this+32];                          // == [primary_object+136]
if (r11 != 0) { *out = 0; return; }       // gate 2: a flag byte must be 0
// gate 3: look up a byte in a large external table
big_table_ptr = *(0x827435F8);            // fixed global, a pointer
flag = big_table_ptr[0x222BFE];           // byte at +2,239,486
*out = (flag != 0) ? 1 : 0;
return;
```

This is not a stub and not a coincidental immediate match — it is a
three-gated readiness poll, structurally exactly what a loading screen's
per-tick "am I done yet" check looks like: an internal state must already
be `1` (own state machine reached "loading" phase), a flag must still be
clear (not already resolved), and then the real question — a single byte,
read fresh from a large table behind a fixed global pointer
(`0x827435F8`), at a fixed 2.14 MB offset (`0x222BFE`) into whatever that
table is. Two of the three gates are locally checkable; the third is an
external readiness signal this report does not trace to its writer.

## Consequence for the plan

This is the loading-screen mechanism the plan's own Phase 2 anticipated
("chargement mission... `NtReadFile` synchrone sur `DATA00.PAC` 177 Mo"),
found from the opposite direction — not by watching file I/O, but by
following the one live broadcast the forced route keeps sending. The
`M150` loop is not an inert animation tick and not a dead interface: it is
`CModeTaskLoadingDemoOffline` asking, every single tick, "is my resource
ready yet," and every measured tick it either fails gate 1/2 locally or
reads a still-clear flag at `[global_827435F8][+0x222BFE]`. The render
queue's starvation and the task list's silence (`994109dc`) are downstream
of this: nothing constructs a menu/mission task because the loading task
that would hand off to one is parked polling a flag that never flips.

**This turns the campaign's open question from "why is nothing
constructed" into a single, concrete, locally-checkable one: what should
set that flag, and why doesn't it, on this route.** The two possibilities
this report does not distinguish: (a) the flag is a real VFS/resource
load-completion signal and the port has a genuine gap in whatever sets it
(a kernel call, an APC completion, a decode step) — matching the plan's own
named risk; or (b) gates 1/2 (`[primary+12]==1`, `[primary+136]==0`) are
themselves never satisfied on this route in the state this report observed,
making the table read moot — not distinguished without a live read of those
two fields, which this report did not capture.

## Not established

- The live values of `[0x2E3C0200+12]` and `[0x2E3C0268+32]` on this route
  — whether the poll ever reaches its third gate at all, or fails at gate 1
  or 2 every tick. Needs a targeted live read (a new narrow watch, or a
  one-off memory dump at a mid-loop tick), not attempted this cycle.
- What `[0x827435F8]` points to, what writes to it, and what (if anything)
  is supposed to set the byte at offset `0x222BFE` within it.
- Whether this same flag/table is checked by any other, currently-reachable
  code path, or is specific to this loading-task family.
- Whether `CModeTaskLoadingDemoOffline`'s construction path itself (what
  called its constructor, and from where in the `menu_endMode`-forced
  chain) matches what the *natural* route would do if it ever reached this
  point — not traced; `003daa94` already showed the natural tick-3001 route
  never gets this far.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean before
this commit. No source changed — both instruments used pre-existed this
cycle (`AC6_DEMO_WATCH_SWG_NATIVE_CALL`, `AC6_DEMO_WATCH_SWG_LISTENER_ARRAY`).
