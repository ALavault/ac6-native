# Cycle 1325 — it was never a matrix builder

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing was executed this cycle: two
  disassemblies were compared as text, and three constants were read.
- No product C++ changed.

## `0x822A1E80` does not build a matrix

Twenty cycles treated it as the matrix assembly. It is **40 instructions with no
vector arithmetic at all** — three constant vectors copied out of `.rodata`, then
three calls:

```
r31 = r3 + 0x80
f31 = f1 ; f1 = f2 ; f30 = f3          the three angles, shuffled
[r31+0x10] = [0x8204F7F0]              (1, 0, 0, 0)
[r31+0x20] = [0x8204F800]              (0, 1, 0, 0)
[r31+0x30] = [0x8204F810]              (0, 0, 1, 0)
bl 0x820A9B30   (r3 = r31, f1 = the caller's f2)
bl 0x820A99F8   (r3 = r31, f1 = the caller's f1)
bl 0x82211828   (r3 = r31, f1 = the caller's f3)
```

The three constants were read, not assumed: `3f800000` in successive lanes.
**They are the identity basis.**

So the contract is: *reset the basis at `object+0x10` to the identity, then apply
three rotations.* Three things follow that no amount of further VMX128 testing
would have produced.

**The angles are applied in the order f2, f1, f3.** The composition order is not
the argument order, and a port that assumed otherwise would be wrong in a way
that looks right at any single-axis test.

**Nothing writes `r31+0x00..+0x0F`.** The 16 bytes at `object+0x00` are untouched
by this function, so a fourth row — or a translation — comes from elsewhere.

**The identity at zero angles was never a property of this function.** It writes
the identity unconditionally, before any angle is looked at. Whether it survives
is entirely a property of the three callees. Cycle 1303 read the right addresses
and its own table says so: `+0x90/+0xA0/+0xB0` hold the identity from step 26 to
step **180**, and break at 184 — which is inside `0x820A9B30`, not here.

That vindicates the instruction to stop assuming the identity, and locates the
defect one level down, where none of the last twenty cycles was looking.

## The two engines agree on it, instruction for instruction

`tools/audit_recomp_vs_ghidra_listing.py` compares Ghidra's listing with the
`// mnemonic operands` comments XenonRecomp emits above each instruction it
generated. Nothing runs; this is the cross-match use `CLAUDE.md` permits.

**40 of 40 for `0x822A1E80`.** The engines print the same instructions in four
notations, and **every equivalence the tool applies is listed, restricted and
counted**, because a comparison that quietly accepts a spelling difference will
quietly accept a real one:

```
immediate radix x26        or rA,rB,rB -> mr x3     vrN -> vN for N < 32 x6
mfspr/LR -> mflr x1        subi -> addi (negated) x3
mtspr/LR -> mtlr x1
```

Two negative controls, because a comparator that cannot fail proves nothing: a
different function's listing under the same symbol diverges at index 1, and one
altered operand — `addi r31,r3,0x80` to `0x84` — diverges at index 8.

A fifth equivalence was needed for the callees and it is **not** cosmetic:
XenonRecomp prints `lvlx v13,0,r8` where Ghidra prints `lvlx v13,r0,r8`. The
recompiler renders the `(rA|0)` rule **already applied**. That is a third
independent statement that the rule exists — after the ISA and cycle 1296's
measurement — from a tool that had to get it right to generate working code, and
the SLEIGH module is the one that does not.

## And then the engines stop agreeing, on a value

At the first `vpermwi128` in each of the three callees:

| site | Ghidra | XenonRecomp |
|---|---:|---:|
| `0x820A9BEC` | `0xAC` | `0xB7` |
| `0x820A9AB8` | `0xEC` | `0xE3` |
| `0x822118E0` | `0x8C` | `0x8F` |

This is a **decode** difference, not a spelling one, and it is decidable. Scoring
both against the `_mm_shuffle_epi32` the recompiler emitted at those very sites,
under all four candidate readings of the lane order:

```
high-first, reversed storage     ghidra 0/3   recomp 3/3
high-first, direct storage       ghidra 0/3   recomp 0/3
low-first,  reversed storage     ghidra 0/3   recomp 0/3
low-first,  direct storage       ghidra 0/3   recomp 0/3
```

Ghidra's immediate reproduces the emitted shuffle under **no** reading. The
recompiler's reproduces it under **exactly** the one cycle 1314 adjudicated over
545 sites and Xenia's four conformance vectors.

And the raw encoding settles it independently. Taking bits 24 and 25 of the
instruction word as the immediate's bits 6 and 5, and bits 11–15 as its low five:

```
0x19B76350 -> 0xB7    0x19A363D0 -> 0xE3    0x19AF6310 -> 0x8F
```

Three for three, against the recompiler and against Ghidra. **The top bit is not
determined by this sample** — all three sites are `v13,v12`, so the bit I read it
from is constant across them — and that is stated rather than papered over.

## Which means the harness override has been reading a wrong immediate

This is a **fifth** module defect and it is worse than the lane order, because
the lane order was worked around and this corrupts the workaround's input:
`applyOverride` takes the immediate from Ghidra's `Instruction` API.

**And the suite could not see it.** `vpermwi128-override` computes its expected
value from the same Ghidra immediate the override reads. Fixture and subject
share a source, so they agree no matter what that source says — the twenty-seventh
shape, for the third time in this campaign, and this time in code I wrote after
writing the shape down.

Cycle 1314's 545/545 cross-match is **unaffected**: it read the immediate from
XenonRecomp's own comments and compared against XenonRecomp's own emitted
shuffle, and never used Ghidra's value.

## Not established

- The immediate's top bit, and therefore the full field layout.
- Whether the field formula holds beyond three sites. It is decidable over the
  whole corpus and that is the next cycle.
- What the three callees compute. They are 77 instructions each, all three the
  same length, and their streams agree with the recompiler up to the `vpermwi128`
  immediate.
- What occupies `object+0x00..+0x0F`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none
ctest                                100% passed, 0 failed out of 29
tools/tests                          Ran 72 tests, OK
recomp_vs_ghidra (0x822A1E80)        pass, 40 instructions, 40 equivalences named
```

No contract changed, so no evidence needed re-pinning.

## Next

Validate the immediate's field layout over the whole `vpermwi128` corpus — the
recompiler's comments give 545 expected values and Ghidra gives 545 words, so the
formula is checkable rather than arguable, and the top bit falls out of any site
with a different `VD`. Then correct `applyOverride` to decode the immediate from
the instruction word instead of trusting the module, and rebuild the suite's
expectations from a source the override does not share.

Only then the three callees, and only then a matrix.

Nothing is ported until that is done: A3.1's `RetailTransformKernel` would be
built on an instrument that is currently, measurably, feeding itself wrong
immediates.
