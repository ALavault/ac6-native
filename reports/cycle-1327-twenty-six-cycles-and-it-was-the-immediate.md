# Cycle 1327 — twenty-six cycles, and it was the immediate

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No console emulator, no bridge, no game run.
- No product C++ changed. The kernel is qualified, not yet ported.

## `0x822A1E80` returns the identity at zero angles

```
identity-zero   exit=return  steps=547  vpermwi128 overrides fired=6
    row0 [1.0, 0.0, 0.0, 0.0]
    row1 [0.0, 1.0, 0.0, 0.0]
    row2 [0.0, 0.0, 1.0, 0.0]
```

The residual defect that stood from **cycle 1300 to cycle 1326** is resolved, and
it was the `vpermwi128` immediate: the module decodes it wrongly at 536 of 545
sites, and every one of the six sites in this closure was among them.

Cycle 1303 called the identity "the known answer"; cycle 1304 corrected that to
"an expectation derived from structure". The expectation was **right**, and for a
reason neither cycle had: `0x822A1E80` writes the identity itself, and the three
rotations preserve it at zero. The instruction to stop assuming it was still
correct — the assumption was unsupported at the time it was made, and being lucky
is not being right.

## All three callees are exact plane rotations

Twelve sentinel cases, an asymmetric basis with all twelve words distinct, four
angles including zero. Each callee leaves **one row untouched** and rotates the
other two in their plane. The model is checked lane by lane rather than admired:

| callee | called with | fixes | plane | sign | worst deviation |
|---|---|---:|---|---:|---|
| `0x820A9B30` | the caller's **f2** | row 1 | (0, 2) | −1 | 1.5e-06 |
| `0x820A99F8` | the caller's **f1** | row 0 | (1, 2) | +1 | 1.2e-06 |
| `0x82211828` | the caller's **f3** | row 2 | (0, 1) | +1 | 1.2e-06 |

```
a' =  a·cos + sign·sin·b
b' = -sign·sin·a + b·cos
```

**12 of 12 within 2e-05**, and exactly `0.0` at every zero-angle case. The middle
axis carrying the opposite sign is what a consistent right-handed convention does
to it — that is an observation about the measured signs, not a convention
imported from outside.

So the contract of `0x822A1E80`, complete:

```
reset the basis at object+0x10/+0x20/+0x30 to the identity
rotate about row 1 by f2      (0x820A9B30, called FIRST)
rotate about row 0 by f1      (0x820A99F8, called SECOND)
rotate about row 2 by f3      (0x82211828, called THIRD)
```

The composition order is **not** the argument order, and no single-axis test
would have shown it.

## The bug this cycle wrote, and what caught it

The first version of the capsule generated its `override` lines from a **window**
around the entry point — `function ± 0x400`. `0x822A1E80` contains no
`vpermwi128` in its own 0x400 bytes, so the case ran with **no overrides at all**,
its three callees executed on the module's wrong immediate *and* wrong lane
order, and the rows came back as twelve zeros.

Nothing in the snapshot said why. `overrides_fired` was 0 and that is exactly
what a correct run with no `vpermwi128` in range looks like.

This is the risk the ladder names in one sentence: with the override chosen over
a SLEIGH patch, **a forgotten site is a silent error**, so a spec must never be
scoped by hand. The fix is not a wider window. An override only fires when the PC
reaches its address, so the specs now list **all 545 sites**, generated from the
census, and the class of error is gone rather than narrowed.

It was caught because the sentinel cases — which name their callee directly and
so did get their overrides — returned their input unchanged at angle zero while
the assembler case returned zeros. Two cases that must agree, disagreeing.

## The harness gained `dump`

A region seeded with `bytes:` cannot also carry a poison fill, so until now a
pre-seeded object could be written but never read back. `dump NAME` emits a
region's final bytes for **both** poison passes, so a reader can see for itself
that a seeded region's result does not depend on the poison rather than take it
on trust. Every case here reports `poison_independent`.

What is traded away is stated: write detection. `dump` gives every value and
loses "written" versus "left alone".

## Not established

- What the object's first 16 bytes are. `0x822A1E80` never writes them and the
  sentinel `[17, 29, 43, 61]` came back untouched in every case.
- Whether the three rotations are the only writers of the basis.
- The algebraic composition. Each call transforms the rows in place, so the three
  compose in the order given; writing that as a product of matrices requires a
  row-versus-column convention this cycle did not measure, and it is not written
  down here as though it had been.
- Nothing is ported. `RetailTransformKernel` is the next cycle.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none
ctest                                100% passed, 0 failed out of 29
vmx128_behaviours                    pass, 32/32
transform_kernel                     pass, 12/12 rotation cases, worst 1.5e-06
tools/tests                          Ran 72 tests, OK
```

## Next

`RetailTransformKernel` — **one** native boundary, shared by flight orientation
and rendered-unit orientation, with these three rotations and their order behind
it. The differential already exists: twelve sentinel cases and an identity case,
and a native implementation has to reproduce all thirteen before it earns a
contract entry.
