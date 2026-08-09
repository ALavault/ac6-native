# Cycle 1314 — the oracle was not needed, 545 of 545

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon` for the suite re-run.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** XenonRecomp's generated C++ was read as
  **literal cross-match evidence about an instruction's semantics** — the use
  `CLAUDE.md` permits. Nothing was executed and no game behaviour was observed.
- No product C++ changed.

## What cycle 1306 got wrong

It concluded that `vpermwi128`'s lane order *"needs an execution oracle"* and
stopped the thread on that basis. The reasoning was sound on its evidence — two
documentations disagreeing, the discriminating immediates absent from the image —
and the conclusion was still too strong. **A third source was in the workspace
the whole time.**

## The adjudication

XenonRecomp generated C++ for **this XEX**, and every `vpermwi128` became an
`_mm_shuffle_epi32` whose immediate re-encodes the PPC one. There are four
candidate readings: the high or the low bit-pair selecting element 0, crossed
with the recompiler storing PPC element 0 at the low or the high x86 lane.

`tools/audit_vpermwi128_crossmatch.py` scores all four over the whole corpus:

| reading | explains |
|---|---:|
| **high-first, reversed storage** | **545 / 545** |
| high-first, direct storage | 2 / 545 |
| low-first, reversed storage | 1 / 545 |
| low-first, direct storage | 0 / 545 |

545 sites, 33 distinct immediates, one reading. The margin is what makes it an
adjudication rather than a preference: a rival that explained 400 would be an
argument, one that explains 2 is noise.

Two details worth keeping:

- **545 is exactly the site count cycle 1306 measured in the image.** Two
  independent instruments — a text scan of the disassembly and a regex over
  generated C++ — agree on the population, which is a control neither was built
  to provide.
- The reading confirmed is the one Xenia's *code* implements, the one the
  harness override already used, and the opposite of what the SLEIGH module does.
  The module's source is local at
  `.tools/ghidra-xenon-extension-source/data/languages/vmx128.sinc:1416`, where
  `sel_0 = perm & 0x3` is assigned to `vregD[96,32]` — the high lane. It is
  wrong, and now demonstrably.

## A correction to accept, and one to make

**Accepted:** *swizzle* is the better word than *permute*. `vpermwi128` allows
duplication — `0x00` gives `[x,x,x,x]`, `0xFF` gives `[w,w,w,w]` — which a
permutation in the mathematical sense does not. The ladder now says swizzle.

**Corrected:** it was put to me that Xenia "documents and tests precisely these
four cases". It documents them; it does not test them. `grep -rln vpermwi` over
`src/xenia/cpu/testing/` returns nothing — what exists is `swizzle_test.cc`,
which tests the `Swizzle` HIR opcode. Cycle 1309 recorded this and it stands.
It no longer matters: the cross-match is stronger than a four-case conformance
test would have been, because it covers the corpus rather than four points.

## What changed in the repository

- `tools/audit_vpermwi128_crossmatch.py`, with
  `reports/mission01-retail/vpermwi128-crossmatch.json`.
- `tools/audit_vmx128_behaviours.py`: the entry moves from *readings disagree* to
  a confirmed module defect. The `disputed` machinery is removed — three pinned
  defects, all confirmed. **23/23 after the change.**
- `MISSION01_LADDER.md`: the instrument section's "unresolved, and it needs an
  oracle" becomes "resolved, and it needed no oracle".

## What this does and does not unblock

It removes one unknown from every vector chase that follows, and it retires the
only item the plan listed as needing an oracle.

**It does not fix `0x822A1E80`.** Cycle 1306 measured that high-first, applied at
all six sites with the register-file bridge on, still does not return the
identity at zero angles. That was true before this cycle and is true after: at
least one more defect sits on that path. What changed is that the hunt now has
one fewer suspect and no longer waits on anything external.

## Not established

- What the residual defect in `0x822A1E80` is.
- Whether the module mis-implements any other instruction the same way. Three are
  measured; the module has many more.

## Gates

```
mission01_final_gate (playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
vmx128_behaviours=pass (23/23, 3 confirmed module defects)
vpermwi128_crossmatch=pass 545/545, best rival 2/545
tools/tests: Ran 72 tests, OK
```

## Next

A7: `0x821CAA50`, and the narrow question first — does it write the four `0xA0`
records at `0x826EDBA0`, and from what.
