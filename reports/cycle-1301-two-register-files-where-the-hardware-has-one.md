# Cycle 1301 — two register files where the hardware has one

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The cause, found

Walking `0x8209CB70` put the answer in the emulator's own warnings:

```
WARN Uninitialized register read at 8209cc58: vr4
WARN Uninitialized register read at 8209cc58: vr5
```

`8209cc58` is `vmulfp128 vr13,vr4,vr5`. Fourteen and twenty instructions
earlier, `8209cc44 vspltw v5,v13,0x2` and `8209cc50 vspltw v4,v0,0x3` **wrote
exactly those two registers**. The emulator says they were never written.

On Xenon there are 128 vector registers and both instruction families address
the same ones: the AltiVec forms name them `v0..v31`, which this module calls
`vsNN`, and the VMX128 forms name them `vr0..vr127`. `vs32+n` and `vrn` are one
storage on hardware.

**They are not in this module.** One instruction, seeded and read both ways:

| seeded | instruction | `vs37` after | `vr5` after |
|---|---|---|---|
| `vs45` = `0f0e…0100` | `vspltw v5,v13,0x2` | `0x07060504` ×4 | **all zero** |
| `vr13` = `0f0e…0100` | same | all zero | all zero |

The splat lands in `vs37` and `vr5` stays empty; seeding the VMX128 name instead
leaves the instruction with nothing to read. **The two families are disjoint
storage.**

## What that explains

- **Why the composite is input-independent.** Every value an AltiVec-form
  instruction produces is invisible to the VMX128-form instruction that consumes
  it. The dataflow of any real routine is severed at each crossing, and what the
  VMX128 side reads instead is a constant — so the output cannot vary with the
  input. Six cycles chasing that invariance; this is it.
- **Why cos was right and sin was wrong (cycle 1300).** The cos chain is
  pure-`vr`: `lvx128 vr13` → `vmulfp128 vr0,vr13,vr13` → `vmsum4fp128 vr7,vr0,vr7`,
  never leaving the family. The sin chain crosses through
  `vspltw v4`/`v5` into `vmulfp128 vr13,vr4,vr5`, and dies there. Two outputs of
  one routine, one correct and one not, from the same 92 instructions — which
  cycle 1300 recorded as the puzzling part and is now the confirming detail.
- **Why `vpermwi128`'s override changed nothing (cycle 1298).** It fixed a real
  defect on a dataflow that was already cut.

## And why sixteen passing tests could not see it

Cycle 1298 wrote "every vector instruction in the closure is correct" and cycle
1299 extended it to all 179 sites. Both were true **within a register file** and
structurally blind to the boundary between them.

Each case seeds and captures using the naming the instruction's own p-code uses:
`vmrghw` was seeded `vs42`/`vs40` and captured at `vs38`; `vmulfp128` was seeded
`vr0`/`vr13` and captured at `vr12`. **No case ever crossed.** Sixteen green
tests over eleven mnemonics, and the defect sat in the one place none of them
looked.

This is the shape worth writing down: *a suite whose fixtures inherit the
subject's own convention cannot test that convention.* The cases were not weak
individually — the flaw was that they were all built the same way, so their
blind spot was shared rather than averaged out. It is the twenty-seventh shape
and belongs in `INSTRUMENT_DISCIPLINE.md`.

It is now case 17, pinned as `register-file-alias`, and the third pinned module
defect.

## Not established

- Whether the disjointness is total or only affects some register numbers. One
  pair was tested, `vs37`/`vr5`.
- Whether any AltiVec-form instruction in this module writes `vr` rather than
  `vs` — the split was inferred from two families of p-code output and measured
  on one instruction.
- What `0x822A1E80` computes. Still no claim, and it will stay that way until
  the harness can execute the routine without the severance.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (17/17, 3 pinned module defects)
```

## Next

Bridge the two files in the harness. The override mechanism from cycle 1298
already reaches any address, and the four CALLOTHER behaviours already write
their own output — so both can write the alias as well as the varnode the module
gave them. The control is ready-made: re-run the three angle triples and, for the
first time in seven cycles, the matrix should move when the angles do.
