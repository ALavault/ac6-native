# Cycle 1329 — 460 of 460

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing was executed: two disassemblies were
  compared as text.
- No product C++ changed, no contract changed.

## A function `exports/` recovers six instructions of

`CLAUDE.md` records that `0x822A23D8` is 460 instructions and that `exports/`
silently truncates it to **6**. It is one of the four callers of the transform
kernel, and A3.2 cannot start without reading it.

It is now qualified end to end: **460 of 460 against XenonRecomp**, with
`.pdata` independently declaring 460 and `check_listing_against_pdata.py`
reporting `short=0`. The comparator needed five more equivalences to get there,
each named, restricted and counted:

```
immediate radix x416          or rA,rB,rB -> mr x15
vrN -> vN x50                 clrlwi -> rlwinm x7
mtspr/CTR -> mtctr x7         subi -> addi (negated) x3
(rA|0) rendered as 0 -> r0 x6 vupkd3d128 vD,IMM,vB -> vD,vB,IMM x1
```

## A correction to yesterday

Cycle 1328 restricted `vrN → vN` to **N < 32**, reasoning that `vr32..vr127` have
no AltiVec spelling so rewriting them would invent one. True of the ISA, false of
the recompiler, which simply calls all 128 registers `vN`. The restriction met
`vr127` against `v127` at `0x822A23F0` and reported a naming difference as a
divergence. The index still has to match, which is the part that carries meaning.

## The known defect is classified, not equated

Ghidra decodes `vpermwi128`'s immediate wrongly at 536 of 545 sites. Every
function carrying one diverges here, and quietly rewriting it would hide a defect
this campaign measured. So the comparator **reports it by address and counts it
separately**, and the run may still pass — the same treatment the vector suite
gives its pinned defects.

The tolerance is narrow and a control proves it: changing a `vpermwi128`
**register** — not its immediate — still fails at that instruction. Two other
controls also still bite after nine equivalence rules: a different function's
listing under the same symbol diverges at index 1, and one altered operand at
index 8.

The three rotation functions now read `pass, 77 instructions, known_defects=2`
each — six sites, which is the whole closure.

## The transform block

`0x822A1E80` does `r31 = r3 + 0x80`. Its caller `0x822A23D8` does `mr r3,r31`
before the call and `stvx128 v0,r31,r11` with `r11 = 192` after — so the vector
lands at `r31+0xC0`, which is **`transform+0x40`, the same block**:

| offset | contents |
|---|---|
| `+0x00` | never written by the kernel; the sentinel `(17,29,43,61)` survived all fourteen runs |
| `+0x10` `+0x20` `+0x30` | the three basis rows |
| `+0x40` | a vector whose lanes 1 and 2 are `[r30+0x08]` and `[r30+0x0C]`, lane 3 a `.rodata` constant |

And the angles at that call site:

```
f1 = [r30+0x18]    applied SECOND, about row 0
f2 = [r30+0x1C]    applied FIRST,  about row 1
f3 = f31           applied THIRD,  about row 2
```

## What is deliberately not named

**`r30` is not called a state, a pose or a unit.** It is read as floats at
`+0x04`, `+0x08`, `+0x0C`, `+0x10`, `+0x14`, `+0x18` and `+0x1C`, and that is all
this cycle established. Cycle 1299 paid four cycles for an unexamined premise of
exactly that kind.

**`f3`'s origin is not traced.** It is a non-volatile register set earlier in the
460 instructions, and I did not follow it.

**The other three callers are not analysed.** `0x822A1E80` has four call sites.
`0x822955F0` applies `fneg f1,f1` before one of its two calls, and `0x8230B030`
builds `f2` by adding a `.rodata` constant — so the angle conventions may differ
per caller, and nothing here says they do not. The kernel is shared; its callers'
conventions are not yet known to be.

## VX128_P is closed, by a source and a corpus agreeing

An answer arrived mid-cycle with Xenia's `InstrData::VX128_P` field positions, and
it is checkable rather than merely credible. Written out as masks — bitfield
placement is an ABI property, not an ISA one — the declared layout is:

```
25..21 VD128l   20..16 PERMl   15..11 VB128l
10 fixed 0   9 fixed 1   8..6 PERMh   5 fixed 0   4 fixed 1
3..2 VD128h   1..0 VB128h
```

Cycle 1326 reconstructed the PERM bits from the corpus alone, consulting no
documentation. The two statements are **the same eight bits**:

```
imm[7] = ppc word[23] = x86 bit  8 -> PERMh[2], declared at x86 bit  8   AGREE
imm[6] = ppc word[24] = x86 bit  7 -> PERMh[1], declared at x86 bit  7   AGREE
imm[5] = ppc word[25] = x86 bit  6 -> PERMh[0], declared at x86 bit  6   AGREE
imm[4..0] = ppc word[11..15]       -> PERMl[4..0], x86 20..16           AGREE
```

And the declared layout was then applied over the corpus, including the parts the
derivation never looked at:

```
declared_layout_reproduces_recomp=545/545
declared_layout_structural_bits=545/545
ghidra_registers_match_declared=545/545
```

The **structural bits are the control**: bits 10 and 5 are declared constant 0
and bits 9 and 4 constant 1, and if the field positions were wrong those would
not be constant across 545 encodings. They are.

**This narrows the module defect, and narrowing it is worth as much as
confirming it.** Cycle 1326 listed the register fields as *not established*, and
they now are: Ghidra decodes `vD` and `vB` correctly at every site. **Only the
immediate is wrong.**

`VX128_P` comes off the open-questions list.

## Not established

- What `r30` is, and what lane 0 of the `+0x40` vector holds.
- Whether the four callers agree on the angle convention.
- Nothing in the product changed. A3.2 has a map and no code.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
recomp_vs_ghidra  0x822A1E80         pass, 40 instructions
recomp_vs_ghidra  0x820A9B30/99F8/1828  pass, 77 each, 2 known defects each
recomp_vs_ghidra  0x822A23D8         pass, 460 instructions, 2 known defects
vpermwi128_immediate_decode          pass, declared layout 545/545, registers 545/545
```

## Next

`f3` and lane 0, then whether the other three callers share the convention —
because a shared kernel with four different angle conventions above it is the
same failure the single kernel was built to prevent, one level up.
