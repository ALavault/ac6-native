# Cycle 1299 — every vector instruction is correct, and the answer is not

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The closure's vector layer is now fully tested

Cycle 1298 left three mnemonics untested over 49 sites. All three are done, and
the suite is **16 of 16**.

| mnemonic | sites | verdict |
|---|---:|---|
| `vmsum4fp128` | 42 | correct |
| `vmrghw` | 39 | correct |
| `lvx128` | 28 | correct; **defective when `rA = r0` and `r0 ≠ 0`** |
| `stvx128` | 18 | correct |
| `vmrglw` | 12 | correct |
| `lvlx` | 12 | correct, three alignments |
| `vrlimi128` | 9 | correct |
| `vmulfp128` | 7 | correct |
| `vpermwi128` | 6 | **reversed**; overridden, no effect |
| `vspltw` | 3 | correct |
| `vor` | 3 | correct |

`stvx128` is the first case checked through `memory_writes` rather than a
register, which is the only way to test a store; the tool grew `expect_write`
for it.

## The latent defect is now measured, and still does not fire

Cycle 1296 read `lvx128`'s p-code emitting `INT_ADD(r0, rB)` with no `(rA|0)`
rule and recorded it as latent because `r0` was zero on the path. Seeding
`r0 = 0x10` at `0x822A1EC0` turns latent into measured:

```
ISA:    0x000102030405060708090a0b0c0d0e0f
module: 0x101112131415161718191a1b1c1d1e1f
```

The module adds `r0` and lands one block further on. Pinned as the second
`module_defect_actual`.

**And it does not fire here.** `r0` was probed through the composite at steps 26,
200, 400 and 547 and is `0x00000000` at every one — the sin/cos does not dirty
it. A defect that is real, measured, and irrelevant to this path is worth
exactly that much, and no more.

## The fourth row was poison, and that was not it either

The caller writes three matrix rows, at `+0x90`, `+0xA0` and `+0xB0`. A 4×4
would have a fourth at `+0xC0`, and `+0xC0` was inside the poison region — so if
the callees read it, they read `0xCD` bytes as floats in one pass and `0x00` in
the other.

Splitting the object into `poison:0xC0` plus a defined zero row at `+0xC0`
changes **nothing**: the three angle triples still produce byte-identical output.
The control was worth running and it came back negative.

## Where that leaves the search

Everything in the vector layer is eliminated. So are the caller (correct at step
26, cycle 1296), the sin/cos (`cos(0.75)` to the bit on the callee's frame,
cycle 1296), `r0`, and the poison tail. The answer is still the same three rows
for every input.

**And the reframing this forces is uncomfortable.** Four cycles have assumed the
output should be a rotation matrix varying with three Euler angles. That
assumption comes from `MISSION01_LADDER.md`'s description of `0x822A23D8`, which
cycle 1295 already refused to treat as established — and it has been the
unexamined premise of every experiment since. It is entirely possible that
`0x822A1E80` behaves correctly for the state it was given and that **the
expectation is what is unverified**, not the execution.

What survives the reframing is the input-independence itself: whatever the
function computes, it takes three floats through three separate calls and its
only observable output does not move when they do. That is a fact about the run,
not about the expectation.

## Not established

- What `0x822A1E80` computes. Five cycles; still no claim.
- Whether the expectation of a rotation matrix is right at all.
- Whether the three floats reach the sin/cos as *its* angle argument. The
  caller's `fmr` chain routes them into `f1` at each call site, and `cos(0.75)`
  was measured on the frame — but for the third call only, and the other two
  were never read back.
- Whether `vmsum4fp128` is correct at all 42 sites. One wiring was tested.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_artifacts=pass cited=31 match_head=31
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (16/16, 2 pinned module defects)
```

## Next

Stop testing the machinery and test the premise. Read back the sin/cos results
for **all three** calls, not the last, and check each against the angle that call
was given. If all three are right, the function is receiving what it should and
producing something that is not a function of it — which would be a statement
about the function, and the first one this thread has been able to make.
