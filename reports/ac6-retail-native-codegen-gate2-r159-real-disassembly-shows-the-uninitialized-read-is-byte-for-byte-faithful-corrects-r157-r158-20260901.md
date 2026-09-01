# AC6 retail NTSC-U/J — real Ghidra disassembly of `sub_82338568`/`sub_823382A8` shows the uninitialized `[r1+88]` read is byte-for-byte faithful to the retail binary; corrects r157/r158's "recompilation-specific" framing (r159)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`
(sufficient here — both functions disassembled are short and fully
captured, well under `Ac6XenonDisasm`'s 300-instruction cap; verified by
inspection, not by `.pdata` since neither address has a `.pdata` row).
XEX US `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Real, static disassembly of the retail binary itself — **not** the
generated XenonRecomp C++ (which this project's own discipline treats as
"literal cross-match evidence only, never a source for native behaviour").
No native code changed, no build touched this cycle.

## Why this check was worth running

r157/r158's own "Next" named a targeted comparison at the exact `[r1+88]`
read as the priority follow-up to their oracle findings. Before attempting
any runtime-level oracle comparison (materially harder: it would need
Xenia Edge internals access this project does not have), the cheaper,
higher-priority check is the one this project's own discipline already
calls for whenever a suspected bug sits in generated code: verify the
XenonRecomp C++ this whole DATA.TBL arc (r130-r153) has been reading is
actually a faithful translation of the real retail binary, rather than
assuming it and moving straight to oracle-level comparison.

## What was found: the generated C++ is byte-for-byte faithful

`Ac6XenonDisasm` against the real XEX at `0x82338568` (`sub_82338568`):

```
82338568  mfspr r12,LR
8233856c  stw r12,-0x8(r1)
82338570  std r31,-0x10(r1)
82338574  stwu r1,-0x70(r1)        ; 112-byte frame, matches -112(r1)
82338578  or r31,r3,r3             ; r31 = handle
8233857c  addi r3,r1,0x50          ; r3 = r1+80
82338580  bl 0x823382a8            ; call sub_823382A8
82338584  lis r11,-0x7dcc
82338588  addi r5,r1,0x50          ; r5 = r1+80
8233858c  subi r4,r11,0x7d98
82338590  or r3,r31,r31
82338594  bl 0x82339d10            ; call sub_82339D10
82338598  cmpwi r3,0x0
8233859c  bge 0x823385c4           ; if r3>=0, take the "discard" path
...
823385c4  li r4,-0x1
823385c8  lwz r3,0x50(r1)          ; r3 = [r1+80]  (the handle sub_823382A8 wrote)
823385cc  bl 0x821f4128            ; side-effect call, result discarded
823385d0  ld r31,0x58(r1)          ; 64-BIT LOAD from [r1+88] -- never written
823385d4  b 0x823385a4             ; return r31
```

`0x58 = 88` decimal. **This is the exact instruction r141/r142 identified
and r153 re-confirmed via the generated C++** —
`ld r31,88(r1)` — present, unconditionally, in the real retail binary's own
compiled code, not introduced or altered by XenonRecomp's translation.

`Ac6XenonDisasm` against `0x823382a8` (`sub_823382A8`, the function that
initializes the buffer at `[r1+80]` before the call):

```
823382a8  mfspr r12,LR
...
823382bc  li r11,0x0
...
823382d0  stw r11,0x4(r31)         ; writes offset +4
823382d4  bl 0x821f5798
823382d8  or r11,r3,r3
823382dc  li r10,0x1
823382e0  or r3,r31,r31
823382e4  stw r11,0x0(r31)         ; writes offset +0
823382e8  stw r10,0x4(r31)         ; writes offset +4 again
823382ec  addi r1,r1,0x60
...
823382fc  blr
```

Only offsets `+0` and `+4` (relative to `r31`, which is the same pointer as
the caller's `r1+80`) are ever written. **Offset `+8` — the caller's
`[r1+88]` — is never touched, confirmed against the real instructions, not
just the generated C++.** This matches r153's own reading of the generated
C++ exactly, with no discrepancy.

## What this corrects

r157 and r158 characterized the DATA.TBL stall as **"confirmed
recompilation-specific"** — implying a faithful translation of the real
retail binary would not exhibit it. **That framing is not accurate and is
corrected here.** The real retail binary's own compiled code — verified now
by direct Ghidra disassembly, the most authoritative source this project
has — genuinely contains this exact "read a stack slot nothing initialized"
instruction sequence. There is no XenonRecomp translation bug here: the
generated C++ this whole arc has read since r130 is a faithful, accurate
reproduction of what the real Xbox 360 binary actually does.

**What r157/r158's oracle result still correctly establishes**: the real
game does not *hang* or *crash* here in practice, on real (or faithfully
emulated) execution — Xenia Edge's own boot sequence proceeds cleanly past
this exact code. What r157/r158 got wrong was attributing that to a
difference in the *code being run*; the corrected explanation is a
difference in the *garbage each environment's own execution history leaves
at that exact stack address* — this project's native recompilation, Xenia
Edge's HLE interpreter, and real Xenon hardware each have their own
independent prior stack-reuse pattern, all reading the same genuinely
uninitialized memory, each getting different bytes. r150's own framing
("the value was never fixed at 0 by anything in the traced code — it was
whatever the stack happened to hold, and this session's fixes changed what
the stack holds") was the correct characterization all along; r157/r158
over-interpreted a real, valid oracle observation (the real game boots
fine) into an incorrect claim (that our recompilation's code is unfaithful)
that this cycle's static check does not support.

## What this means for a fix

Since the real binary's own code relies on uninitialized stack content by
design (or by a real, shipped bug in the original game — either way, not
this project's translation to fix), a legitimate native-runtime lever is
narrower than r157/r158 implied: not "correct a mistranslation" (there is
none), but potentially "reproduce whatever prior-execution stack-reuse
pattern real hardware/a working emulator happens to leave here" — which is
a much harder, less well-defined target, bordering on the kind of
synthetic/hardcoded value this project's own discipline explicitly refuses
(r53's precedent, cited in CLAUDE.md). Whether this is worth pursuing
further is a judgment call for the next cycle, not resolved here.

## Decision

This is recorded as a direct correction of r157 and r158, by cycle number,
per this project's own discipline. The underlying oracle observation (real
game boots past this point) stands and remains valuable; the causal
interpretation attached to it does not, and is corrected. `git blame`-style
self-correction: r157/r158 (this same session) drew a conclusion the
generated-C++-vs-real-disassembly check available all along would have
caught immediately, and should have been run before writing "confirmed
recompilation-specific" rather than after.

## Gates

No native code changed, no build touched. `ctest`'s last-known state (9/9,
r155) stands unaffected. `git status` unchanged (only pre-existing,
unrelated dirty state).

## Next

1. Decide whether "reproduce real hardware's likely stack-reuse pattern
   here" is worth pursuing given the risk of drifting into a
   synthetic/hardcoded value this project's discipline refuses — this is
   a judgment call, not yet made.
2. r157/r158's own oracle capture remains valid, qualified evidence that
   the real title boots correctly; it no longer supports the specific
   claim that this project's translation is at fault for the DATA.TBL
   stall.
3. Both of Gate 2's other named frontiers are unchanged: `sub_82390880`/
   `sub_821F5630` closed (r156); `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
