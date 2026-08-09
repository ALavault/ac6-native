# Cycle 1317 — the four suspects are eliminated

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The aliasing hypothesis, and it is dead

Every case in the suite used distinct registers for destination and sources. The
real code does not:

| site | instruction | alias |
|---|---|---|
| `0x820A9C2C` | `vmrghw v0,v0,v7` | D == A |
| `0x820A9BC8` | `vmrglw v8,v11,v8` | D == B |
| `0x820A9BF0` | `vpermwi128 vr12,vr12,0x2c` | D == B |

An implementation that writes the destination lane by lane while still reading a
source clobbers itself, and a suite that never aliases cannot see it — the
twenty-seventh shape wearing a different face.

**All three pass.** The two CALLOTHER behaviours read both operands into
temporaries before writing, by construction; and the module's own `vpermwi128`
survives because its SLEIGH body computes four locals before assigning any of
them. Suite is **26/26**, and the aliased module case is pinned as a fourth
confirmed defect — the same low-first reversal, unchanged by the aliasing.

## The whole suspect list, closed

The remaining candidates for the `0x822A1E80` defect were, in order:

1. **`vmsum4fp128`'s broadcast.** Tested at cycle 1305: `[1,2,3,4]·[10,20,30,40]`
   returns `300.0` in **all four lanes**, which is the broadcast contract.
   `vmsum3fp128` does not occur in this closure — the mnemonic census found 42
   `vmsum4fp128` and no three-way form.
2. **Merge aliasing.** This cycle. Dead.
3. **`vrlimi128`'s rotations and masks.** The image carries exactly two immediate
   pairs, `0x4/0x3` and `0x3/0x2`; both are tested and both are correct
   (cycles 1295, 1304).
4. **`lvx128`/`stvx128`'s memory contract.** Tested as **two independent halves**
   rather than a round trip: `lvx128` is seeded from memory and read out of a
   register, `stvx128` is seeded from a register and read out of memory, each
   compared in memory order.

   That is deliberately stronger than the round trip a round trip would suggest.
   **A round trip cancels a convention error** — swap the byte order on load and
   again on store and the bytes come back identical while every lane in between
   is wrong. Two half-tests, each anchored against memory, cannot cancel.

**So all eleven vector mnemonics in the closure are tested, aliased and
unaliased, and none of them is wrong** — except `vpermwi128`, whose defect is
measured, adjudicated (cycle 1314) and overridden.

## Which relocates the problem

The residual defect in `0x822A1E80` is **not a per-instruction semantics error**.
Two possibilities remain and this cycle does not choose between them:

- **the harness setup** — something the spec seeds, or fails to seed, that the
  routine reads. The object, the stack, `r0` and the poison tail have each been
  controlled; the four `0x1164` objects and whatever `0x820A9B30` expects of its
  caller have not.
- **the expectation** — cycle 1304 already corrected cycle 1303 for calling the
  zero-angle identity "the known answer" when it is an inference from the
  routine's *structure*. The structure argument is good and it is still an
  argument.

That is a real narrowing: eleven instructions and four hypotheses are off the
list, and what is left is not in the ISA layer at all.

## Not established

- The residual defect.
- Whether `r20` is zero throughout `0x821CAA50` — cycle 1316 left this open and
  every offset in that report depends on it. It is the next thing, and it is
  cheap.

## Gates

```
mission01_final_gate (playable-v1)  JF=pass open=none
ctest: 100% tests passed, 0 failed out of 28
vmx128_behaviours=pass (26/26, 4 confirmed module defects)
contract_artifacts=pass
tools/tests: Ran 72 tests, OK
```

## Next

Settle `r20` in `0x821CAA50`, because cycle 1316's whole record layout rests on
it and an unchecked assumption under a published layout is exactly what this
campaign keeps paying for. Then the differential for `0x821CAA50` — it is
scalar, the record array can be a poison region, and one run yields the flag word
and the float array together.
