# Cycle 1303 — two rows wrong, one right, and a hypothesis refuted

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The known answer, and where it stops being one

At zero angles `0x822A1E80` must leave the matrix as the identity. Walking the
bridged run against that:

| steps | `+0x90` | `+0xA0` | `+0xB0` |
|---:|---|---|---|
| 26 – 180 | `1 0 0 0` | `0 1 0 0` | `0 0 1 0` |
| **184** – 188 | `1 **1** 0 0` | `0 1 0 0` | `0 0 1 0` |
| **192** – 547 | `1 1 0 0` | `0 1 0 0` | `0 **1** 0 0` |

Two transitions, each inside a three-step window, and each one a store in the
**first** callee `0x820A9B30`:

- steps 181–183 contain `820a9c30 stvx128 vr0,r0,r31` → row 0
- steps 189–191 contain `820a9c4c stvx128 vr12,r0,r10` → row 2

**Row 1 is never wrong.** `820a9c48 stvx128 vr13,r0,r11` writes `0 1 0 0` and it
stays. The later two callees change nothing at zero angles, which is correct —
a rotation by zero should be a no-op, and for them it is.

So the corruption is upstream of two of three stores, in the `vmrghw` network
that assembles `vr0` and `vr12`, and it spares the one that assembles `vr13`.
A known-answer input localised in one pass what four cycles of unknown-answer
inputs could not.

## A hypothesis, and its refutation

The bridge takes the registers to copy from `Instruction.getResultObjects()`.
If Ghidra reports nothing for a CALLOTHER instruction — one whose semantics it
declined to model — the bridge would be blind to exactly the four operations the
harness supplies itself, `vmrghw` among them. That would explain two merge
chains going wrong while a third survived.

It does not happen. One instruction each, seeded and read both ways:

| instruction | kind | `vs` after | `vr` after | copies |
|---|---|---|---|---:|
| `vmrghw v6,v10,v8` (`0x820998E8`) | CALLOTHER | `0x001122330f0e0d0c…` | **same** | 2 |
| `vspltw v5,v13,0x2` (`0x8209CC44`) | module | `0x07060504…` | **same** | 2 |

Both mirror. The hypothesis is dead and cost one run.

## What the copy count says about the defect

`copies=2` for a single written register means `getResultObjects()` returns
**both names** — the write is reported as `vs38` and as `vr6`, and the bridge
copies in each direction.

So the module's **operand model already knows the two names are one register**.
It is only the emulator's **storage** that keeps them apart. That narrows what
the defect in the SLEIGH module is: not a missing equivalence in the register
definitions as such, but two register spaces carved where the operand layer
expects one.

It also means the bridge is doing something the module half-declares, rather
than inventing an equivalence — which is worth having on record, since the
bridge is asserted machinery on which every result after cycle 1302 rests.

## Not established

- What corrupts `vr0` and `vr12`. The window is three instructions wide in each
  case, and the merge network above them is not yet read.
- Why `vr13`'s chain survives. It may be luck at zero angles rather than a
  structural difference; nothing here distinguishes the two.
- Whether the residual defect is the module's or the bridge's. Both are still
  possible: the bridge is mine, and cycle 1302 is the first result that depends
  on it.
- What `0x822A1E80` computes.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
instrument_discipline_index=pass shapes=18 unindexed=0
```

## Next

Capture `vr0`, `vr12` and `vr13` immediately before their three stores, together
with the `vmrghw` inputs that feed them — `v13`, `v5`, `v7` for `vr0`; `v12`,
`v9`, `v11` for `vr12`. With `vmrghw` validated and the merge arithmetic known,
the three rows can be computed by hand from those inputs and compared. Whichever
of the three disagrees names the instruction, and whether `vr13` survives by
structure or by luck falls out of the same capture.
