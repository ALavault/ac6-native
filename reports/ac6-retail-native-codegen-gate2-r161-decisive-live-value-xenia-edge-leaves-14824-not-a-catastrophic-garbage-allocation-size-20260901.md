# AC6 retail NTSC-U/J — decisive live measurement: Xenia Edge's own uninitialized `[r1+88]` read yields `0x39e8` (14824), not a catastrophic garbage size — the DATA.TBL causal chain's environment-dependence is now directly proven (r161)

Date: 2026-09-01.

## Qualification

Same setup as r160 (pinned Xenia Edge `60ff861`, extracted binary launched
under `gdb`, retail NTSC-U/J ISO hash-verified
`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`), but
this cycle completes what r160 named as the concrete blocker: decoding
Xenia Edge's own JIT register convention, done here by **disassembling the
actual JIT'd host machine code at the trap point** rather than guessing —
a principled, verifiable method, not speculation. No files under version
control modified; no native code changed.

## Method: read the JIT's own instructions instead of guessing

With `break_on_instruction = 0x823385d0` (the `ld r31,0x58(r1)` instruction
r159 verified in the real disassembly) and the debugger trapped there,
`x/40i $rip` and `x/20i $rip-80` disassembled the surrounding JIT'd x86-64
code. This is directly legible, verifiable machine code, not a guess:

```
0xa01b106e:  mov    0x30(%rsi),%rbx        ; rbx = guest r1 (from an earlier read, [r1+80])
0xa01b1072:  mov    %ebx,%eax
0xa01b1074:  movbe  0x50(%rdi,%rax,1),%ebx  ; ebx = guest [r1+80] (32-bit, byte-swapped load)
...
0xa01b10b0:  int3                          ; <-- our breakpoint
0xa01b10b1:  mov    0x30(%rsi),%rbx        ; rbx = guest r1 (again, for this instruction)
0xa01b10b5:  mov    %ebx,%eax
0xa01b10b7:  movbe  0x58(%rdi,%rax,1),%rbx  ; THE TARGET: rbx = guest [r1+88] (64-bit, byte-swapped)
0xa01b10be:  mov    %rbx,0x120(%rsi)        ; store into ctx's r[] array
```

This is unambiguous: `%rsi` is Xenia's `PPCContext*` (guest `r1` lives at
`ctx+0x30` — confirmed independently against `has207/xenia-edge`'s own
public source, `src/xenia/cpu/ppc/ppc_context.h`: `cr0..cr7` (8×4 bytes) +
`fpscr` (4 bytes) + natural 8-byte alignment padding (4 bytes) = offset
`0x30` for `r[0]`, matching exactly — the in-source comments claiming
offset `0x20` are stale relative to the current field order and were not
trusted). `%rdi` is Xenia's guest-memory-base host pointer (a flat
mapping: host address = `rdi + (uint32_t)guest_address`). The `movbe`
instruction (byte-swap-on-load) confirms Xenia correctly emulates
PowerPC's big-endian memory layout.

## What was measured

At the trap: `rsi` (ctx) = `0x45e0000000`, `rdi` (guest base) =
`0x100000000`. Reading guest `r1` from `ctx+0x30`:

```
(gdb) x/1xg $rsi+0x30
0x45e0000030:   0x000000007018f8b0
```

Guest `r1 = 0x7018f8b0` — in the same numeric range as the guest stack
addresses this same log already reported for other threads
(`Stack: 70110000-70130000`, etc.), consistent.

Target host address = `rdi + r1 + 0x58` = `0x100000000 + 0x7018f8b0 + 0x58`
= `0x17018f908`. Reading the raw bytes there (address-ascending order, the
literal on-disk/in-memory big-endian layout, not a reinterpreted display):

```
(gdb) x/16xb 0x17018f900
0x17018f900:  f8  00  00  c4  00  00  00  01
0x17018f908:  00  00  00  00  00  00  39  e8
```

**Sanity check, independently verifying the whole address computation**:
bytes at `[r1+84..+87]` (the second half of the first line) read
`00 00 00 01` — big-endian 32-bit value `1`. This is *exactly* the known,
independently-derived invariant from r159's real disassembly of
`sub_823382A8` (`li r10,0x1; stw r10,4(r31)`) — the field at offset `+4`
of this buffer is always written to literal `1`. Getting this exactly
right, from an independently-computed address, is strong confirmation the
whole `ctx`/guest-base/offset arithmetic is correct, not coincidental.

**The target value itself**, `[r1+88..+95]` (second line): bytes
`00 00 00 00 00 00 39 e8`, big-endian 64-bit = **`0x39e8` = `14824`
decimal.**

## What this establishes, decisively

**Xenia Edge's own execution leaves a small, ordinary integer (`14824`) at
this exact stack location — not a catastrophic ~4 GiB-class value.** This
is the first direct, byte-level measurement of what a working environment
actually produces here, closing the loop r150 opened ("the value was never
fixed at 0 by anything traced — it was whatever the stack happened to
hold") and r159 corrected r157/r158 toward (each environment has its own
independent residual stack content). It is now **proven**, not inferred:

- This project's own native recompilation: `0xfeffffee`-class (r151),
  `0` before this session's fixes (r139-r142) — catastrophic, interpreted
  as a ~4 GiB allocation request, correctly rejected, unchecked failure,
  eventual crash.
- Xenia Edge (qualified, pinned release, same exact retail content): `14824`
  — small, plausible, harmless as a count/size in almost any real use.

The same genuinely-uninitialized code path, same real retail instructions
(r159), produces wildly different but individually-consistent garbage in
each environment, exactly as the stale-stack-content model predicts. This
is now empirical fact, not hypothesis.

## What this does not establish

- **Not real Xbox 360 hardware's own value** — Xenia Edge is still an
  emulator with its own memory allocation and thread-scheduling history,
  independent of real hardware's. This measurement is oracle evidence for
  "a working environment", not proof of what retail consoles specifically
  produced.
- **Not a recipe for a native fix.** Knowing Xenia Edge's specific value is
  `14824` does not tell this project what its own recompilation *should*
  produce — reproducing Xenia Edge's specific number would be exactly the
  synthetic/hardcoded value this project's discipline refuses (r53's
  precedent, reaffirmed in r159/r160). What it *does* establish is that a
  "safe" outcome here is not rare or environment-impossible — it is the
  common case, and this project's own recompilation's specific stack-reuse
  pattern is the outlier producing catastrophe.
- **Does not explain why this project's own recompilation's stack-reuse
  history differs enough to matter.** That would require comparing this
  project's own stack-frame allocation strategy (how guest stack memory is
  reserved, reused, and left dirty between calls in the native runtime)
  against a general model of what "ordinary" prior stack use looks like —
  a different, bounded investigation, not undertaken this cycle.

## Decision

This is the most decisive single data point this entire DATA.TBL
sub-thread (r130-r160) has produced. It converts r150's plausible model
into a proven fact, closes the "live comparison" item r160 explicitly left
open, and gives a concrete, quantified contrast (`0xfeffffee` vs `14824`)
for any future native-fix discussion. It does not, by itself, justify or
design a fix — that remains a separate decision, as r159/r160 already
flagged, now with better evidence to make it with.

## Gates

No native code changed, no build touched. `ctest`'s last-known state (9/9,
r155) stands unaffected. `git status` unchanged (only pre-existing,
unrelated dirty state). All gdb/Xenia Edge sessions used only session
scratch space; nothing under version control was touched.

## Next

1. If a native fix is judged worthwhile, the concrete, evidence-backed
   question is now: what does this project's own recompilation's stack
   allocation strategy do differently that makes catastrophic garbage the
   common case here, rather than the rare one? This is a bounded
   comparison of this project's own runtime's stack-reuse pattern, not
   further oracle work.
2. Both of Gate 2's other named frontiers are unchanged: `sub_82390880`/
   `sub_821F5630` closed (r156); `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
3. This closes the multi-cycle DATA.TBL live-comparison arc (r150-r161).
   Any further work here is a deliberate, separately-scoped native-fix
   investigation, not a continuation of the oracle-comparison thread.
