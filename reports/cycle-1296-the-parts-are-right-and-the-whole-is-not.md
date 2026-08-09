# Cycle 1296 — the parts are right, and the whole is not

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** Documentation read for instruction meaning:
  the VMX128 opcode reference at `biallas.net/doc/vmx128/vmx128.txt`, Xenia's
  `ppc_emit_altivec.cc`, and the Cell BE SIMD PEM. No game code ran, no game
  behaviour was observed.
- No product C++ changed.

## The isolation cycle 1295 owed

Cycle 1295 wrote four vector behaviours, validated none individually, and
debugged them through a 547-step composite that localised nothing.
`tools/audit_vmx128_behaviours.py` is the missing half: **one retail
instruction per case**, every input seeded, the output register captured,
compared against a value worked out by hand and written down beside it.

The harness grew three directives for it — `region … bytes:HEX` for inline
fixtures, `vec NAME HEX` to seed a vector register, `capture vec:NAME` to read
one back — plus `steps 1`, which was already there.

| case | site | result |
| --- | --- | --- |
| `vmrghw` | `0x820998E8` | pass |
| `vmrglw` | `0x820998FC` | pass |
| `vrlimi128` | `0x820A9B98` | pass |
| `lvlx+0`, `lvlx+4`, `lvlx+12` | `0x820A9B8C` | pass |

**6 of 6.** The checker also refuses a case that matched without the behaviour
firing, because a captured register still holding its seed would otherwise pass
in silence.

Two questions closed by that: the four behaviours are not the fault, and
`defaultSpace()` **is** the space the module's `LOAD` uses — cycle 1295 listed
that as untested and as the thing that would produce exactly its symptom. The
`lvlx` cases read the fixture at `0xB6000000` correctly at three alignments.

## So where is it

**The caller is correct.** Stopping the composite at step 26, right after its
last store and before the first call:

```
r10 = 0x8204f810
vr0 = 00000000000000003f80000000000000
```

and the image at `0x8204F810` is `00000000000000003f80000000000000` — the same
bytes. The object then holds a clean identity at `+0x90/+0xA0/+0xB0`. So
`lvx128`, `stvx128` and the constant loads all work.

**The arguments reach the arithmetic.** Making the stack a poison region instead
of a zero one shows the callee's own frame at `r1+0x54` holding `3f3b4fcd` —
`cos(0.75)` to the bit, for the third call, whose argument is `f3 = 0.75` — and
`r1+0x5c` holding the negation of `r1+0x50`. `0x820A9B30` calls a fourth level,
`0x8209CB70`, at `820a9b50` with two stack pointers in `r3`/`r4`: that is the
sin/cos, and it ran.

So the corruption is **inside the callees, downstream of the sin/cos and
upstream of the final store**, and it is explained neither by the four
behaviours nor by the caller. That is a much smaller box than cycle 1295 left.

## Two module defects, named, and one of them is unreachable

Reading the caller's p-code turned up the class of defect the Ghidra issue
(NationalSecurityAgency/ghidra#2094) tracks:

```
822a1ec0  lvx128 vr0,r0,r11
    unique = INT_ADD(r0:8, r11:8)          <- no (rA|0) rule
    unique = INT_AND(unique, ~0xF)
    vr0:16 = LOAD(ram, unique)

822a1ed4  lis r10,-0x7dfb
    r10:8 = INT_LEFT(0xffffffffffff8205:8, 0x10:4)   <- sign-extended base
```

The first is the same omission cycle 1295 had to repair by hand for `lvlx`: in
an indexed form `rA = r0` means the literal zero, and the module emits a
register read. **Neither bites here** — `r0` is zero on this path and the load
resolved to the right bytes, as the probe above shows — so both are recorded as
latent, not as the cause.

But the first one carries a consequence for the whole method: `lvx128` has real
p-code, so there is **no CALLOTHER to hook**, and a defect in an instruction the
module implements directly is not interceptable by this harness at all. The
asserted-semantics mechanism only reaches what the module declined to implement.
That is a ceiling on the technique and it is worth knowing before the technique
is leaned on.

## What an isolation test does not prove

It validates a behaviour against my model *given the operand order I assumed*.
If the module passed `(vB, vA)` where I read `(vA, vB)`, my test would pass and
the semantics would still be wrong.

The order is corroborated separately, and the corroboration is weaker for two of
the four than for the others:

- `vrlimi128` — settled by decoding the instruction word in cycle 1295
  (`0x19846FD0` → `IMM=0x4, z=3`), independent of any printed operand.
- `vmrghw` / `vmrglw` — only from the disassembler's operand text
  (`vmrghw v6,v10,v8` against `CALLOTHER(vs42, vs40)` = `(v10, v8)`). That is
  the same module whose defects this cycle just recorded.
- `lvlx` — the fixture is asymmetric, so a swapped `(rA, rB)` would have
  produced a different address and failed. This one the test does cover.

## Not established

- What `0x822A1E80` computes. Unchanged from cycle 1295, and no claim is made.
- Which instruction inside the callees loses the result.
- Whether `0x8209CB70` or anything below it needs an operation not yet supplied.
  Nothing faulted, so nothing is missing outright, but "did not fault" is not
  "is correct".
- Whether the module's `vmrghw`/`vmrglw` operand order matches the ISA. The
  encoding was not decoded for these two.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: 47 tests, OK
vmx128_behaviours=pass (6/6)
```

## Next

Decode the `vmrghw`/`vmrglw` encodings to close the operand-order gap, then walk
the callee `0x820A9B30` instruction by instruction with `steps N` and a vector
capture, which is now cheap and localises to a single instruction. The box is
small enough that bisecting it is a bounded job rather than a search.
