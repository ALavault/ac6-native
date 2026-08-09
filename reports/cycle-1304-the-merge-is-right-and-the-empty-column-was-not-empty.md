# Cycle 1304 — the merge is right, and the empty column was not a defect

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The merge network, checked by hand

Capturing the `vmrghw` inputs at each step around the first store:

| step | instruction | before | after | `vmrghw` says |
|---:|---|---|---|---|
| 181 | `820a9c24 vmrghw v0,v13,v5` | `v13 = (1,1,1,1)`, `v5 = (0,0,0,0)` | `v0 = (1,0,1,0)` | `{v13₀,v5₀,v13₁,v5₁}` = `(1,0,1,0)` ✓ |
| 183 | `820a9c2c vmrghw v0,v0,v7` | `v0 = (1,0,1,0)`, `v7 = (1,0,1,0)` | `v0 = (1,1,0,0)` | `{v0₀,v7₀,v0₁,v7₁}` = `(1,1,0,0)` ✓ |

The row that gets stored is exactly what the merge should produce from the
values it was given. **The assembly is not the fault** — the values arriving at
it are.

## The empty column was not empty of anything

At step 165 the four operands of the dot products are
`vr8 = (1,0,0,0)`, `vr11 = (0,1,0,0)`, `vr9 = (0,0,1,0)`, **`vr10 = (0,0,0,0)`**.

Three unit vectors and a zero one is exactly the shape of "one column never got
populated", and it was one sentence away from being published as the defect.

**Reading the image refuted it.** The table the callee loads from:

```
0x8204F7E0  00000000000000000000000000000000   0 0 0 0
0x8204F7F0  3f800000000000000000000000000000   1 0 0 0
0x8204F800  000000003f8000000000000000000000   0 1 0 0
0x8204F810  00000000000000003f80000000000000   0 0 1 0
0x8204F820  0000000000000000000000003f800000   0 0 0 1
```

`0x8204F7E0` is a **genuine zero vector**, deliberately placed at the head of a
`0x10`-stride table of basis vectors, and `820a9ba8 lvx128 vr10,r0,r9` loads it
on purpose. `vr10 = 0` is correct.

That is the shape this file keeps recording: a clean, complete, plausible
observation that means something else. The cost of checking was one read.

## And the transpose is right too

`0x820A9BB8`–`0x820A9BE4` is the AltiVec 4×4 transpose idiom. At zero angles it
produces `vr8 = c₀`, `vr11 = c₁`, `vr9 = c₂`, `vr10 = c₃` — and the merge order
derived from the listing consumes them as
`(·vr8, ·vr11, ·vr9, ·vr10)`, which is `(c₀, c₁, c₂, c₃)` **in order**. The
distribution across registers looked like a permutation and is not one.

## `vrlimi128`'s second immediate pair

Nine `vrlimi128` sites in the closure: six are `0x4/0x3`, which the suite already
covered, and **three are `0x3/0x2`** — including `0x820A9BC4`, which builds the
rotation row this cycle localises the corruption to. One immediate pair is not a
test of an immediate.

Added at `0x820A9BC4`: mask `0x3` takes elements 0,1 from `vD` and 2,3 from the
`vB` rotated left by two words. **Correct.** Suite is 18/18.

## A correction to cycle 1303

It called the zero-angle identity "the known answer". It is not known — it is
an expectation, and cycle 1299 recorded exactly this mistake about exactly this
function. What is solid is narrower and comes from the **structure** rather than
from anyone's description: the callee loads three rows from the object,
transposes them, dots them against a row built from `sin`/`cos`, and stores the
result back. That is a matrix product, and a product with a rotation of zero must
return what it was given. The expectation is well founded; it is still an
expectation, and the report should have said so.

## Not established

- What corrupts the result. Everything downstream of `vr13` is now measured
  correct: the transpose, the columns, the constant table, both `vrlimi128`
  immediate pairs, the merge network, the stores. **The remaining suspect is
  `vr13` itself** — the rotation row, built by `820a9ba4` and `820a9bc4` from
  `lvlx` reads of the `sin`/`cos` stack slots — and it has not been captured
  before the dot products consume it.
- What `0x822A1E80` computes.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (18/18, 3 pinned module defects)
```

## Next

Capture `vr13` immediately before `820a9bfc`, the first dot product that consumes
it, at zero angles where it should be a unit row. Its three inputs — the `lvlx`
loads at `820a9b8c`, `820a9b94`, `820a9b9c` from stack `+0x58`, `+0x54`, `+0x5c`
— are cheap to capture in the same run, and cycle 1300 already measured what
those slots hold. If `vr13` is wrong and its inputs are right, the two
`vrlimi128` compose wrongly despite each being correct alone, which is the last
place left.
