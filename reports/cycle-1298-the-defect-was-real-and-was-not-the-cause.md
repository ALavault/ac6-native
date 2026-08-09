# Cycle 1298 — the defect was real, and was not the cause

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** Xenia's `ppc_emit_altivec.cc` read as
  documentation of instruction meaning. No game code ran.
- No product C++ changed.

## The override, built and controlled

Cycle 1297 measured `vpermwi128` returning the ISA's answer reversed, and there
is no CALLOTHER to hook because the module implements it itself. The harness now
takes `override ADDR NAME`, recognised in the step loop exactly the way a stubbed
call already was: the PC is matched, the replacement runs, the PC advances four.

**What is replaced is the semantics, not the decode.** The module's decode is
corroborated for this instruction — its own p-code materialises the immediate as
`0xac`, and the register operands match the disassembly — so the operands are
read back through the `Instruction` API. Re-deriving them from the encoding was
tried and abandoned: the VMX128 reference's `PERM` field would not reassemble by
hand across the two sites, and guessing it would have put an unmeasured decode
underneath a measured semantics.

The control covers the whole chain including the decode, because a wrong
register would read an unseeded one:

```
vpermwi128-override: 0x0706050407060504030201000f0e0d0c   (the ISA answer)
cases=11 passed=11 failures=0 pinned_module_defects=1
```

## And it changed nothing

All six `vpermwi128` sites in the closure — `0x820A9AB8`, `0x820A9ABC`,
`0x820A9BEC`, `0x820A9BF0`, `0x822118E0`, `0x822118E4`, two per callee —
overridden, three angle triples, `override:vpermwi128` firing 6 times each:

| angles | `+0x90` | `+0xA0` | `+0xB0` |
|---|---|---|---|
| `(0,0,0)` | `0 0 1 0` | zero | zero |
| `(0.25,0.5,0.75)` | `0 0 1 0` | zero | zero |
| `(π/2,0,0)` | `0 0 1 0` | zero | zero |

Byte-identical to the runs without the override, and still input-independent.
**`vpermwi128` is a real defect and not the cause.** Fixing a measured bug and
watching the symptom not move is the ordinary case, and it is worth saying so
plainly rather than quietly moving on.

## The census, redone by mnemonic

Cycle 1297 established that counting CALLOTHERs measures only what the module
declined to implement loudly. Counting **mnemonics** over the closure's
disassembly instead:

| mnemonic | sites | tested |
|---|---:|---|
| `vmsum4fp128` | 42 | one site, correct |
| `vmrghw` | 39 | correct |
| `lvx128` | 28 | — |
| `stvx128` | 18 | — |
| `vmrglw` | 12 | correct |
| `lvlx` | 12 | correct, three alignments |
| `vrlimi128` | 9 | correct |
| `vmulfp128` | 7 | correct |
| `vpermwi128` | 6 | **reversed**, overridden |
| `vspltw` | 3 | correct |
| `vor` | 3 | — |

**Eleven distinct mnemonics, 179 sites.** Four of them were CALLOTHERs. The
instrument cycles 1294 and 1295 sized the problem with was looking at roughly a
third of the surface, and the two operations added this cycle — `vspltw`, the
other lane-indexed instruction and so the obvious second suspect, and
`vmulfp128`, which has no lane order to get wrong and therefore isolates the
arithmetic — are both correct.

## Not established

- What corrupts the matrix. Three cycles have not found it, and the honest
  statement is that it is still open.
- `lvx128`, `stvx128` and `vor` are untested — 49 sites, the largest remaining
  block. `lvx128`/`stvx128` mask the effective address with `~0xF`, which is
  correct AltiVec behaviour, and they carry the same missing `(rA|0)` rule cycle
  1296 recorded as latent.
- Whether `vmsum4fp128` is correct at all 42 sites or only at the one tested.
  Different sites carry different register pairs and the test covers one wiring.
- What `0x822A1E80` computes. Unchanged since cycle 1295; still no claim.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (11/11, 1 pinned module defect)
```

## Next

Test the remaining 49 sites — `lvx128`, `stvx128`, `vor`. If they are all
correct, then every vector instruction in the closure is correct and the fault
is not in the ISA layer at all, which would move the search to the scalar setup:
the `lfs`/`stfs` traffic between the sin/cos and the vector lanes, and the
`(rA|0)` rule on the two memory instructions that carry the most sites.
