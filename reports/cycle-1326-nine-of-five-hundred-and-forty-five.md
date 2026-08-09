# Cycle 1326 — nine of five hundred and forty-five

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The field layout, derived rather than guessed

Cycle 1325 found Ghidra and XenonRecomp decoding different immediates from the
same three `vpermwi128` words, and hand-fitted a layout to three samples — all
three of which named the same registers, so the top bit could not be separated
from the destination field. That fit was **wrong in exactly that bit**, and I
said so at the time rather than shipping it.

`tools/audit_vpermwi128_immediate_decode.py` settles it over all 545 sites, and
it does not propose a layout. For each of the eight immediate bits it asks
**which of the 32 instruction-word bits agrees with it at every site**. A bit
with one candidate is derived; several is under-determined; none refutes the
premise that the immediate is a permutation of word bits. Every bit had exactly
one:

```
imm[7] = word[23]   imm[4] = word[11]
imm[6] = word[24]   imm[3] = word[12]
imm[5] = word[25]   imm[2] = word[13]
                    imm[1] = word[14]
                    imm[0] = word[15]
```

PERMh in bits 23–25, PERMl in bits 11–15, PowerPC bit numbering. Applied back
over the corpus:

```
ghidra_sites=545  recomp_sites=545  paired=545
engines_agree=9/545
derived_reproduces_recomp=545/545
derived_reproduces_ghidra=9/545
```

**The SLEIGH module decodes this immediate wrongly at 536 of 545 sites.** That is
the fifth module defect, and the nine agreements are the coincidences where the
differing bits happen to match.

The recompiler's addresses are recovered as `function start + 4 × index` from its
per-instruction comments. That is only valid if every instruction gets exactly
one comment — which cycle 1325 confirmed at 40 of 40 on `0x822A1E80` **before**
this cycle relied on it.

## What it broke, and why nothing caught it

`applyOverride` existed to correct `vpermwi128`'s lane order. It read its
immediate from `Instruction.getOpObjects(2)` — **from the same module whose
semantics it was correcting**. So for two cycles the harness applied the right
lane order to the wrong immediate.

And `tools/audit_vmx128_behaviours.py` could not see it. Its expected value was
computed from the module's operand too. Fixture and subject shared a source, so
they agreed no matter what that source said — the twenty-seventh shape, third
occurrence, and this one in code written *after* the shape was written down.

Both are corrected. The harness decodes from the instruction word; the six suite
immediates are the derived ones, with the module's value written beside each so
the difference is visible rather than replaced:

| site | word | module | word says |
|---|---|---:|---:|
| `0x820A9AB8` | `0x19A363D0` | `0xEC` | `0xE3` |
| `0x820A9ABC` | `0x199B6390` | `0xCC` | `0xDB` |
| `0x820A9BEC` | `0x19B76350` | `0xAC` | `0xB7` |
| `0x820A9BF0` | `0x199B6250` | `0x2C` | `0x3B` |
| `0x822118E0` | `0x19AF6310` | `0x8C` | `0x8F` |
| `0x822118E4` | `0x198F62D0` | `0x6C` | `0x6F` |

**Six of six differ.** Every `vpermwi128` in the A3.1 closure was being run on a
wrong immediate.

The independent check is now the corpus tool rather than the suite: the suite and
the override still share the derived decode, and pretending otherwise would be
the same mistake with a longer chain. The suite cites the tool instead of
reimplementing it.

## The suite and the calibration

```
vmx128_behaviours              32/32, 4 pinned module defects
microexec_harness_calibration  138/138 equal
```

The pinned `vpermwi128` defect now compounds two errors and the file says so: the
module reads `0xAC` where the word says `0xB7`, **and** assigns the low pair to
element 0.

Cycle 1314's 545/545 cross-match is untouched. It read the immediate from
XenonRecomp's comments and compared against XenonRecomp's emitted shuffle, and
never used Ghidra's value — which is why the lane-order adjudication survives a
defect in the very operand it was about.

## Not established

- Whether `vpermwi128`'s **register** fields are decoded correctly. Both engines
  print the same `vD` and `vB` at the sites compared, which is agreement and not
  proof; the immediate agreed at three sites too, before 545 were looked at.
- What the three callees of `0x822A1E80` compute.
- Everything cycle 1325 left open.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none
ctest                                100% passed, 0 failed out of 29
vmx128_behaviours                    pass, 32/32
microexec_harness_calibration        pass, 138/138
vpermwi128_immediate_decode          pass, 545/545 derived, 9/545 module
tools/tests                          Ran 72 tests, OK
```

## Next

The three callees, `0x820A9B30`, `0x820A99F8` and `0x82211828` — 77 instructions
each, streams already compared against the recompiler up to the immediate. Now
that the immediate is right, re-run the capsule cycle 1303 ran and see whether
the corruption at step 184 survives. If it does not, the residual defect that
stood since cycle 1300 was this, and the identity at zero angles was never the
question.
