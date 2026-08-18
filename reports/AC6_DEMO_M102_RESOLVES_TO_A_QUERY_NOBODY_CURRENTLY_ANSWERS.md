# `"M102"` decodes to message 102 — the only handler found to recognize it never runs in this demo

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence: full-image RTTI walk
against `.build/Default.xex.base.bin` via the new
`tools/rtti_vtable_slot_sweep.py`, and
`codegen/generated/ppc_recomp.{6,9,10,13,14,15,45}.cpp` (control-flow
evidence only). Reachability control: both cached 12000-tick atlases —
START-at-3000 (`a1d0106e`/`39dc4038`,
`/fastdata/lavaulta/tmp/ac6-atlas-title-start/start.atlas.json`) and
neutral/no-input
(`/fastdata/lavaulta/tmp/ac6-atlas-neutral-12000/neutral.atlas.json`).
No oracle.

## What this closes

`AC6_DEMO_EVERY_REGISTERED_LISTENER_AT_THE_MOMENT_OF_SENDMSGI_IS_DEAD.md`
censused the two listeners actually registered when title sends `"M102"`
and found both share a dead `+0x20` slot, but could not settle whether
that slot is unimplemented everywhere ("dead by design") or implemented
elsewhere and simply not reached here ("broken delivery") — it named a
static sweep of every `CSwgListener`-deriving class as the discriminator
and did not run it. This report runs that sweep, decodes the tag itself,
and traces the marshaller's own post-call code far enough to show the
result is not merely dropped silently — it is boxed into a typed value
and hands back an answer the calling script can act on.

## The tag decodes to an integer, and the type-tag wiring is static

Re-reading `sub_820E9388` (the tag validator, first read in
`...NAMED_NONE_IS_COMPLETION.md`) for its exact byte layout:
`buf[0..2] = tag[1..3]` (`'1','0','2'`), `buf[3] = 0` — a proper
null-terminated 3-character string, `"102"`, built by dropping the fixed
`'M'` marker byte and the tag's own 4th validation byte. That buffer is
passed to `sub_8232A100(str, endptr=0, base=10)`, which tail-calls
`sub_823293A8` → `sub_823290B0` — the same core routine `sub_823293C8`
(its unsigned/signed sibling, differing only by one flag byte) also calls,
matching the MSVC CRT `strtoul`/`strtol` pair exactly. Read `sub_823290B0`
past its whitespace-skip/sign/base-prefix prologue to its actual digit
loop (`loc_823292F0`): `r10 = r26*r28` (accumulator × base), `r26 = r10+r11`
(+ digit value, itself `char-'0'` for decimal digits a few lines above,
guarded by an overflow check against `UINT_MAX/base` before every
multiply) — the textbook `strtoul` accumulation, not inferred from the
call shape alone. **`"M102"` is `strtoul("102", NULL, 10)` = 102.**

The command table row itself (`table_row=0x82386478`, captured live by
this session's `AC6_DEMO_WATCH_SWG_NATIVE_CALL` instrument, committed in
`6e8fab2f`, confirmed here to be **static** image data, not
a per-run allocation) is fully static and directly readable:

```
[table_row+0x0] -> "SendMsgI"    (name)
[table_row+0x4] -> "S"            (argument type: one String)
[table_row+0x8] -> "I"             (return type: one Integer)
[table_row+0xC] = 0x820E9838        (the native function, confirmed)
```

## The marshaller boxes a real zero, it doesn't merely drop the call

Re-reading `sub_820E9838`'s own tail (previously only its dispatch loop
had been read): before the loop, it zero-initializes a local
`[r1+84]` (the "handled" accumulator, unrelated to the marshaller's own
`[r1+80]`/`[r1+176]` locals — an earlier draft of this report conflated
the two; corrected here before commit). Every listener call passes
`r5 = &[r1+84]` as an out-parameter. The shared stub (`sub_820AC748`,
re-read here) is a single `blr` — it touches nothing. After the loop
exits, `sub_820E9838` **unconditionally** writes `[r1+84]` through its
own `r4` — the caller-supplied out-param, which happens to be the exact
`r1+80` stack slot in the *marshaller's own* frame (distinct from, not
unrelated to, that slot's other use) — so the `0` that reaches the
marshaller is not a leftover or garbage value; it is `SendMsgI`'s own
deliberately-initialized "nobody answered" accumulator, faithfully
returned.

Reading `sub_820E8F90` (the marshaller) past its `bctrl` at
`0x820E9130`: it loads the return-type tag byte from `[table_row+8]`
(`'I'`, confirmed above) and takes the Integer branch (`loc_820E91DC`):
allocates a 20-byte script value object (`sub_820CE750(20)`), writes the
type tag `20` into it, `[r1+80]` (the returned `0`) into `+12`, and
returns a pointer to this object as the native call's result via `*r22`
(`sub_820E8F90`'s own out-param, the value handed back to the swg script
interpreter). **The script does not merely fail to hear back — it
receives a well-formed, correctly-typed integer value of `0`.**

## Every `CSwgListener`-deriving class in the image, censused

`AC6_DEMO_EVERY_REGISTERED_LISTENER...IS_DEAD.md` only checked the two
classes live at tick 3001. `tools/rtti_vtable_slot_sweep.py`
(`--base-class '.?AVCSwgListener@@' --slot-offset 0x20 --stub-address
0x820AC748`) walks the full RTTI graph instead: finds `CSwgListener`'s
own `TypeDescriptor`, every `BaseClassDescriptor` referencing it
(COMDAT-folded per-`mdisp` group, same folding this campaign's earlier,
narrower reverse-search first ran into), and for each of the **89**
referencing base-class-array slots (88 actual derivers plus
`CSwgListener`'s own self-referential base[0] entry, included by the tool
without being special-cased), resolves the owning class's vtable and
reads `+0x20`.

**Correction, this report, applying to `69ff833a` and `fbd10eef` by
name**: an earlier version of this sweep resolved title/startup to
`0x820113E4`/`0x8201130C` and called them "the wrong vtable" — those
addresses ARE real, but the resolver was finding a different valid
locator sharing the same `ClassHierarchyDescriptor`. Reading the
locator's own subobject-offset field (`locator+4`) settles it: it reads
`104` for `0x82011384`/`0x820112AC` and `0` for `0x820113E4`/`0x8201130C`.
**`0x82011384` and `0x820112AC` — the vtables `69ff833a` dumped and every
report since has called "title's/startup's own vtable" — are actually
the `CSwgListener`-subobject vtables**, and the listener array
(`fbd10eef`) stores `this+104`-adjusted pointers, not raw primary-object
pointers. This is not a new fact changing any prior conclusion — every
live dispatch this campaign traced through `[registered_object+0]+0x20`
used the correct vtable regardless of what it was called — but the label
was wrong, and it retroactively explains `sub_821728C0`'s `addi
r3,r3,-104` (adjusting from exactly this subobject back to the primary
object) as an ordinary, expected multiple-inheritance thunk rather than
an unexplained constant.

With that fixed, the sweep fully resolves (89/89, 0 unresolved — the
resolver's own self-check: title and startup resolve to the exact
vtables this campaign already measured live). **40 of 89 classes
implement a real, non-stub `+0x20` handler.** `CSwgListener`'s interface
is not dead by design; it is a working, actively-used mechanism across
much of the game. Title, startup, `CSelectMessageDlgManager`, and
`CSelectTutorialPopupManager` are simply among the minority that don't
implement it.

## One handler recognizes code 102, checked among 16 of 40 — and never runs

The 40 non-stub implementors resolve to only 18 distinct handler
functions (many classes in the same family share one handler — e.g. 8
`Loading*` classes all point at `sub_8217E258`). Of those 18, 16 have
generated code in this build; grepping each of those 16 bodies' first
~250 lines for a direct `cmpwi r4,imm` / `cmpwi cr6,r4,imm` against 102
finds exactly one match: `sub_82184130`, `CModeTaskMainSelect`'s handler,
which switches on `r4` for codes 100-105. **This is a bounded search, not
an exhaustive one** — it would miss a jump table, a comparison against a
register `r4` was first moved into, or a match past the first ~250 lines,
and it could not check the 2 handlers this build has no generated code
for at all (`0x82180C70`, `0x821855E0` — real, static vtable entries, but
unread bodies). Reading the `r4==102` branch and its shared tail in full:
it loads a menu sub-state value from a fixed global structure, compares
it against 4 and 5, and writes `0` (default), `1` (substate==4), or `2`
(substate==5) into `*r5` — a genuine, meaningful tri-state query, not a
stub with a matching immediate by coincidence.

**`sub_82184130` is confirmed absent from `functions[]` in both cached
12000-tick atlases — the START-at-3000 route and the neutral (no-input)
route** — not reached even once anywhere in either full run, not merely
absent from the tick-3001 snapshot. Among the handlers this report could
read, message 102 has exactly one recognizer, and that recognizer's class
is never constructed on either route.

## Conclusion

The mechanical chain from script to observable result is now fully
traced and, apart from the tick-3001 listener census itself, entirely
static: title's script sends a well-formed `SendMsgI("M102")`; the
runtime decodes this to integer message 102 via the same CRT
`strtoul`-family routine used elsewhere; the listener array holds two
registrants, both a true `blr` no-op at this slot; `SendMsgI`'s own
accumulator, deliberately zero-initialized and never written, is
unconditionally returned; the marshaller — driven by fully static,
directly-read command-table metadata declaring an Integer return type —
boxes that zero into a real script value and hands it back. Message 102
is not an invented or garbage code: among the 16 (of 18 distinct)
`CSwgListener`-interface handlers this report could read, exactly one
recognizes it, and that recognizer's class is never constructed on
either cached route through this run.

**This remains a demonstrated mechanism, not a demonstrated cause.** What
is proven: the script would receive a well-typed `0` if it asks. What is
not proven: that the script's own branch logic reads this particular
return value and that this is what decides "call the completion command
or don't" — that requires the swg bytecode itself, still unlocated. A
live alternative that this report does not rule out: `CModeTaskMainSelect`
may also not be registered at the equivalent moment in **retail** title
processing, if title's transition there is driven by a different path
entirely (this campaign's own memory already documents that the
attract-loop's unprompted advance, `0x8218AB98`, is a separate mechanism
a correctly-timed START suppresses) — in which case `102 -> 0` could be
the normal, harmless answer everywhere, and this whole thread would be
correlation, not causation.

## Not established

- Whether the swg script actually reads and branches on `SendMsgI`'s
  returned integer, as opposed to discarding it — the bytecode itself is
  still unlocated. **Named falsifier for next cycle**: an env-gated host
  intervention that forces the boxed return value to `1` or `2` instead
  of `0` (patching `[r1+84]` right before `sub_820E9838`'s final store)
  and observing, live, whether title's script then goes on to call
  `sub_820EA4A8` (the completion trigger, `642f77a4`). If it does, this
  moves from mechanism to demonstrated cause; if it doesn't, message 102
  was a red herring and the real gate is elsewhere.
- Whether `CModeTaskMainSelect` is ever constructed in the **retail**
  build's equivalent title-screen flow, or whether it too answers `0` at
  the same point — not checked; no retail evidence is used by this
  campaign's rule, and no oracle was consulted.
- Full exhaustiveness of "only `CModeTaskMainSelect` recognizes 102": the
  grep covered only direct `cmpwi r4,imm`/`cmpwi cr6,r4,imm` immediates in
  roughly the first 250 lines of each of the 16 implementors whose
  generated code exists (of 18 distinct handler functions across the 40
  classes) — it would miss a jump table, a `mr rN,r4` then compare in
  another register, or a match past that line window. Two implementors
  (`0x82180C70`, used by 5 Lobby-family classes; `0x821855E0`, used by
  `CModeTaskOptionSelect`/`CModeTaskOptionSelectMission`) have **no
  generated code at all** in this build (checked across every
  `ppc_recomp.*.cpp`) — their vtable pointers are real, static, read
  directly from the image, but their bodies were never read here.
- What the sweep's `unresolved` count would be for a class whose
  `CSwgListener` base sits at array index >14, or under multiple/virtual
  inheritance shapes this resolver doesn't model — none occurred in this
  run (0/89 unresolved), but the resolver was not tested against such a
  shape.

## Gates

New tool `tools/rtti_vtable_slot_sweep.py` — pure static analysis, reads
the XEX image and a caller-supplied base-class/slot/stub triple, no
runtime dependency, no product-code change. Native gate JF, demo ctest,
and both contract audits verified below before commit.
