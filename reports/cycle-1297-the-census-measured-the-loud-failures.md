# Cycle 1297 — the census measured the loud failures

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** Documentation read for instruction meaning:
  the VMX128 opcode reference at `biallas.net/doc/vmx128/vmx128.txt` and Xenia's
  `ppc_emit_altivec.cc`. No game code ran; no game behaviour was observed.
- No product C++ changed.

## The operand-order gap, closed

Cycle 1296 left `vmrghw`/`vmrglw` resting on the disassembler's operand text.
The encodings settle it independently:

| site | word | VD | VA | VB | XO |
|---|---|---:|---:|---:|---|
| `0x820998E8` | `0x10CA408C` | 6 | 10 | 8 | `0x8C` (140, `vmrghw`) |
| `0x820998FC` | `0x1146298C` | 10 | 6 | 5 | `0x18C` (396, `vmrglw`) |

and the CALLOTHERs receive `(vs42, vs40)` = `(v10, v8)` and `(vs38, vs37)` =
`(v6, v5)` — `(vA, vB)` both times. All four asserted behaviours are now
corroborated independently of the module that hosts them.

## Bisecting the callee

A step sweep over the composite, reading the object region at each stop:

| steps | `+0x90` | `+0xA0` | `+0xB0` |
|---:|---|---|---|
| 26 – 180 | `3f800000 0 0 0` | `0 3f800000 0 0` | `0 0 3f800000 0` |
| 185 | `0 3f800000 0 0` | `0 3f800000 0 0` | `0 0 3f800000 0` |
| 190 | `0 3f800000 0 0` | zero | `0 0 3f800000 0` |
| 195 → 547 | `0 3f800000 0 0` | zero | zero |

The three losses are the callee's three stores, `0x820A9C30`, `0x820A9C48`,
`0x820A9C4C`. The identity survives to step 180 — the caller is not at fault,
as cycle 1296 already showed — and the values arriving at those stores are
wrong.

## What the callee's tail actually contains

Reading `0x820A9BA0..0x820A9C60` turned up **two operations neither census
saw**: `vmsum4fp128`, thirteen sites, and `vpermwi128`, two.

They emit **no CALLOTHER**, which is why `Ac6CallOtherCensus.java` reported four
distinct operations for a range containing six. So:

**The CALLOTHER census is the wrong instrument for sizing the gap.** It counts
what the module declined to implement *loudly*. It is blind to what the module
implements *itself* — and those cannot fault, so when they are wrong they
corrupt in silence. Cycle 1294 published 70 operations as "the gap" and cycle
1295 narrowed it to four for this closure; both numbers measured the same wrong
thing. The right instrument is the one this cycle used: test every VMX
instruction on the path, whether the module implements it or the harness does.

## A correction of my own reading, ten minutes old

The p-code for `vmsum4fp128` looked broken — lane 1 reading a register named
`ACC`, lane 3 reading `vr0:4` at the register base rather than word 3, the
output written to three explicit offsets and then to `vr0:4` again. I was ready
to call it the cause.

**Measured, it is correct.** Seeded with `[1,2,3,4] · [10,20,30,40]`, it returns
`300.0` broadcast to all four lanes, exactly. What was wrong was my own
printer: `Ac6PcodeDump.java` labels a sub-register varnode with the enclosing
register's name, so a 4-byte varnode at `vr0+0xC` prints as `vr0:4`. I read a
label and called it a value, which is the one thing `CLAUDE.md` forbids by name.

## The defect, measured

`vpermwi128` is wrong, and it is the first instruction here shown wrong by
measurement rather than by reading.

The reference gives the encoding but not the lane order; Xenia's
`InstrEmit_vpermwi128` gives it — `VD.x = VB[uimm bits 6-7]`, down to
`VD.w = VB[bits 0-1]`, so the **high** bit-pair selects the **first** word. With
`uimm = 0xAC` the selectors are `2, 2, 3, 0`.

```
ISA:    0x07060504 07060504 03020100 0f0e0d0c    {vB2, vB2, vB3, vB0}
module: 0x0f0e0d0c 03020100 07060504 07060504    {vB0, vB3, vB2, vB2}
```

**Exactly reversed.** It is pinned in `tools/audit_vmx128_behaviours.py` as
`module_defect_actual` rather than left as a red test: the file then states a
defect it has measured, and if Ghidra ever fixes the module the case goes red
and says so.

## Endianness, and why the verdict survives it

Two mirror-image vectors are exactly what a big-endian/little-endian confusion
produces, and this whole verdict rests on one convention: that the hex a capture
prints, read left to right, is increasing memory address — so its first word is
ISA element 0.

That convention is **measured, by two controls that were already in the suite
and are now named as such**:

- `lvlx+0` loads sixteen bytes straight out of a fixture and is compared in
  **memory order**. A mirrored capture fails it. `check()` now refuses to report
  any lane-order defect unless this case passed.
- `vmrghw` and `vmrglw` are mirror images of one another with the operands
  swapped: under a mirrored convention `vmrghw(A,B)` reads as `vmrglw(B,A)`.
  Both pass, with distinct expectations and XO-confirmed encodings, which a
  mirrored convention cannot do.

So the reversal is in the module, not in the reading.

## Not established

- That `vpermwi128` is the *only* remaining cause. It is the only one measured
  wrong; `vor`, `stvx128` and the rest of the tail are untested.
- What `0x822A1E80` computes. Still no claim.
- Whether the same lane-order reversal affects other module-implemented VMX128
  instructions. Two were tested; the module has many.
- How many instructions in the wider image are silently mis-implemented. There
  is no census for this, and building one is not obviously possible — which is
  the uncomfortable part of the finding.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (8/8, 1 pinned module defect)
```

## Next

Override `vpermwi128` at the instruction level. There is no CALLOTHER to hook,
but `EmulatorHelper.setBreakpoint` plus `BreakCallBack.addressCallback` reaches
any address, so a module-implemented instruction can be replaced where it sits.
Then re-run the composite and see whether the matrix moves with its input.
