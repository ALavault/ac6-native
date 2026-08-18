# Both EndMode traps are the same shared helper, on two different unlinked objects — plus proof the injection genuinely dispatched

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run. Two things this report
adds to `9bd4e0b9`, both from evidence already on disk: (1) the pre-registered
"did a real opcode fetch happen before the trap" check that report's own
reading depended on but never actually ran, applied now to both existing
`ac6-endmode-inject-run2`/`run3` logs; (2) a static read of the two trap
sites (`lr=0x820DBAF0`, `lr=0x820D771C`) in
`recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.5.cpp`.

## Check 1: the injected offset really was fetched and dispatched as an opcode

`9bd4e0b9` inferred a genuine dispatch from "injection fired, then a trap
occurred" without checking the PC-bracket log's own `generated_line` field in
between — precisely the gap that produced `99044e0f`'s wrong conclusion
(`ab3aed60`). Both injection runs had `AC6_DEMO_WATCH_ADDR_LO/HI` bracketing
the PC field (`0x2E3DFA1C`) the whole time; the check was sitting in the log
unread. Re-reading it now:

**Run2 (offset `0xE04`, target `0x2DCB2024`):**
```
0x2DCB2024  tick=2571  lr=0x82323F2C  sub_823251E0   line=1133   <- injected pc write
0x2DCB2028  tick=2571  lr=0x82325258  sub_82325160   line=1047   <- REAL outer-dispatch fetch
[[[ trap: unmapped 32-bit read, lr=0x820DBAF0, address=0 ]]]
```
Line 1047 is `sub_82325160`'s own outer fetch line (`bfc927e1`) — the same
signature `ab3aed60` established as proof of genuine dispatch, and the one
thing `99044e0f`'s broken v1 never produced. The word at `0x2024` (EndMode's
own opcode `0x16`) was fetched and consumed exactly once, through the real
mechanism, before anything else happened.

**Run3 (offset `0xDF0`, target `0x2DCB2010`):** the same signature, and more
of it:
```
0x2DCB2010  tick=2571  lr=0x82323F2C  sub_823251E0   line=1133   <- injected pc write
0x2DCB2014  tick=2571  lr=0x82325258  sub_82325160   line=1047   <- REAL outer-dispatch fetch
0x2DCB2018  tick=2571  lr=0x82324328  sub_82324320   line=17464  <- a THIRD handler, genuinely entered
0x2DCB201C  tick=2571  lr=0x82324328  sub_82324320   line=17494
0x2DCB2020  tick=2571  lr=0x823243B8  sub_82324320   line=17494
[[[ trap: unmapped 32-bit read, lr=0x820D771C, address=0 ]]]
```
This settles `9bd4e0b9`'s own softer claim ("getting further before failing
confirms `0xDF0` is a distinct code path") with an actual mechanism instead of
an inference from lr alone: `0xDF0`'s word is looked up in the same 104-entry
table and dispatched to a *different* handler, `sub_82324320` — not
`sub_823246C0` (opcode `0x2E`'s handler, the one `0xE04` eventually reaches
downstream) and not a repeat of the same crash. `sub_82324320` runs three of
its own PC-advancing steps (two at line 17464/17494) before the read that
kills it. Both runs show the real dispatcher, table lookup, and (for `0xDF0`)
a full extra handler's own internal loop — genuinely dispatched, not stomped
mid-flight. `9bd4e0b9`'s headline claim stands, now on a checked basis rather
than an inferred one.

## Check 2: both `lr`s are the same shared helper

Grepping the generated code for the two trap addresses:

```
ppc_recomp.5.cpp:18272:  ctx.lr = 0x820DBAF0;   (inside sub_820DBA18, calling sub_820D5B90)
ppc_recomp.5.cpp:8218:   ctx.lr = 0x820D771C;   (inside sub_820D7700,  calling sub_820D5B90)
```

**Both traps' `lr` is the return address into the same callee, `sub_820D5B90`
— called from two different sites on two different objects:**

- `sub_820DBA18` calls it with `r3 = r31` directly (the object it was itself
  handed).
- `sub_820D7700` calls it with `r3 = r31 = (its own r3) + 312` — an embedded
  sub-object 312 bytes into whatever it was itself handed.

`sub_820D5B90`'s own body (`ppc_recomp.5.cpp:4189-4232`) does the same three
things every time it's called, on whatever object it's given (`r4`):

```cpp
ctx.r10 = PPC_LOAD_U32(r4 + 4);           // lwz r10,4(r4)   -- "link" field
ctx.r11 = PPC_LOAD_U32(sext(r10) + 4);    // lwz r11,4(r11)  -- link's own "link" field
cr6.compare(r11, r10);                     // cmplw cr6,r11,r10
if (!cr6.eq) goto loc_820D5BC0;
// twi 31,r0,22                            <- comment only, NO C++ emitted (checked: all
                                            //    ~20 occurrences of this comment in this
                                            //    file are equally empty — the recompiler
                                            //    drops this instruction everywhere)
loc_820D5BC0:
ctx.r3 = ctx.r1 + 80;
ctx.r5 = PPC_LOAD_U64(ctx.r1 + 80);
sub_820D5878(ctx, base);                   // bl 0x820d5878
```

The shape — read a link field, dereference it again, compare to itself — is
the standard idiom for checking whether an intrusive-list node's link points
back to itself (an empty/unlinked sentinel), gated by a debug assert
(`twi 31,r0,22`) that this recompiled build silently never executes (the
codegen elides it everywhere, not just here — this is a general property of
this build, not something specific to EndMode). Both traps land inside this
same function, on its own frame (`lr` in both dumps is still the return
address into the *caller* of `sub_820D5B90`, not into `sub_820D5878` — if
either run had progressed one call deeper, `lr` would read `0x820D5BCC`,
`sub_820D5B90`'s own save point before that call, and it does not, in
either trap).

## What this does and doesn't establish

**Does**: the two "different lr, different code path" traps `9bd4e0b9`
reported are not two unrelated failures — they are the *same* validate-and-
proceed helper, invoked on two different objects that both fail the same way
inside it. That helper is reached from two different opcodes/handlers in
EndMode's own statement chain (`sub_823246C0`'s territory for `0xE04`,
`sub_82324320` for `0xDF0`), which independently converge on it — evidence
that whatever `sub_820D5B90` checks is a shared precondition for more than
one word in this statement group, not something specific to a single opcode.

**Does not**: identify the exact instruction that faults, or which specific
field is null. The register dump at trap time (`r10`, `r11`, `r3`, `r29`,
`r30` etc.) cannot be used to infer which load executed last, because PPC
context registers are a single flat file that persists across the whole
tick — a register showing a "valid-looking" or a zero value at trap time may
be a leftover from earlier, unrelated code, not the result of the faulting
(or a preceding, successful) instruction in this specific call. Pinning the
exact faulting load and the exact null field needs a live instruction-level
trace (e.g., a watch on every `PPC_LOAD_U32`/`PPC_LOAD_U64` call site inside
`sub_820D5B90` itself, not just the PC field), not a static read plus a
post-mortem register dump. Not attempted here.

Also not established: `AC6_DEMO_WATCH_SWG_LOOKUP_KEY` was not enabled for
either injection run, and — checked now — its own tick gate is hardcoded to
`[2990, 8000]` (`AC6_DEMO_CATEGORY_IS_THE_NATIVE_FUNCTION_ID...md`'s own
gates section); tick 2571 falls outside it regardless. There is no
lookup-key evidence around either trap, and there could not have been with
this instrument as currently gated even if it had been turned on. This is a
documented absence, not a finding either way.

## Reading

This sharpens, without changing, `9bd4e0b9`'s conclusion: EndMode's statement
group depends on at least one object being properly linked into some runtime
structure (a list, registry, or similar) before `sub_820D5B90` can validate
it — and in this offline/no-mission run, neither the direct object nor the
+312 embedded sub-object was ever linked. That both failures route through
one shared helper, rather than being two unrelated null derefs, makes
"missing setup" a tighter explanation than "two independent bugs in the
statement's own words" — consistent with, and now somewhat more specific
than, the standing `CX360UnitManager`/mission-scoped-precondition reading.

## Gates

No source changed. Native gate JF, demo `ctest` (26/26), and both contract
audits verified below before commit.
