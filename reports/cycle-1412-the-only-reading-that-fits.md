# Cycle 1412 — the only reading that fits

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- ctest 50 → **51**. Contract 28 → **29 behaviours**.
- New: `tools/audit_scalar_sin_cos_microexec.py`,
  `analysis/flight/scalar-sin-cos-microexec.tsv`,
  `reconstruction/ace-combat-6/{include/ac6,src,tests}/retail_scalar_sin_cos.*`.
- `reports/mission01-retail/transform-kernel-microexec.json` regenerated.

## The seam is closed

Cycle 1411 measured `0x8209CB70` as the only unfaithful scalar substitution in
the flight chain — `std::sin`/`std::cos` differ from it at 242 of 412 values,
worst absolute error 3.20e-07 — and every rotation goes through it. This ports
it. **412 of 412 values, bit for bit, no tolerance.**

92 instructions, a leaf, no calls. A Taylor series to the 23rd order evaluated
four terms at a time in the vector unit, with a range reduction in front of it.

## Two details a readable port would have got wrong

**The reduction is an integer edit.** `rlwimi r11,r10,30,1,31` at `0x8209CBE0`
rotates `0x01243F6D` left by 30, which is **π's bit pattern `0x40490FDB`**, and
inserts bits 1..31 of it over the argument's word — keeping the argument's sign
bit. That is `copysign(π, angle)` written as a bitfield insert, and adding it
before scaling by `float32(1/2π)` turns the truncation that follows into
**round-half-away-from-zero**. `floor(q + 0.5)` rounds toward −∞ and would break
the symmetry between an angle and its negation; the test asserts sine is odd and
cosine even *to the bit*, which is what catches that.

**The subtraction is fused.** `fnmsubs` at `0x8209CC1C` is one rounding, so
`std::fmaf` and not `a - q*2π`.

The three scalars and six coefficient vectors were read from the image and
**checked**, not assumed: the coefficients are exact reciprocal factorials, and
`0x82069BEC` really is `float32(1/(2π))` — not the seven-digit decimal that
cycles 1374 and 1386 each found masquerading as a reciprocal.

## The one thing that had to be arbitrated

`vmsum4fp128` is a four-lane dot product. Its summation order decides the last
ulp and the reference this campaign has does not fix it. So rather than assume:
four association orders crossed with fused and unfused products, each scored
against the executed result at every one of the 412 values.

| reading | exact |
|---|---:|
| **`((p₀+p₁)+p₂)+p₃`, products rounded first** | **412 / 412** |
| the same with fused products | 298 / 412 |
| `(p₀+p₁)+(p₂+p₃)`, rounded | 292 / 412 |
| right-to-left or reversed, either way | 223 / 188 |

One reading reproduces every value; the next best misses 114. That is an
arbitration by cross-match — the standard the `vpermwi128` reading was held to —
and `--arbitrate` re-runs it, so it is reproducible rather than recorded.

## And the build was quietly able to undo it

`((p₀+p₁)+p₂)+p₃` with rounded products is only that if the compiler leaves it
alone. GCC and Clang default to **`-ffp-contract=fast`**, which fuses `a*b + c`
into an `fma` — exactly the reading the arbitration rejected at 298/412.

Nothing in this project had ever set the flag. It is now off globally:

```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-ffp-contract=off)
```

**All 51 tests still pass**, which is itself the result worth having: every other
port already called `std::fmaf` explicitly where retail fuses, so the product was
contraction-independent by construction and nobody had checked.

This is a defect that no differential would have caught, because the differential
compares retail against a *Python* model. The C++ was the unmeasured side.

## What the port bought, measured on someone else's differential

`audit_transform_kernel_microexec.py` computed its expectation with `math.cos`,
so its 2e-5 tolerance was measuring this seam and nothing else. Pointing it at
the port's own model instead:

| | worst deviation |
|---|---:|
| `math.cos` / `math.sin` | 1.4583e-06 |
| **the ported `0x8209CB70`** | **1.0027e-06** |

Tolerance cut from **2e-5 to a measured 1.1e-6**. It is not zero, and the reason
is named rather than absorbed: the tool models a float32 vector composition in
float64. That residue is no longer trigonometry.

## Two errors of mine, both caught by the differential

- The first model took the x⁸ step from the **squared** vector rather than the
  base one — `vspltw v5,v13,2` splats lane 2 of `(1, x, x², x³)`, which is x²,
  and reading it off `(1, x², x⁴, x⁶)` gives x⁴ and a step of x¹⁰. Rejected at
  **336 of 412** values.
- The scratch version of the arbitration scored `LR/unfused` at 412/412 with the
  correct step; the tool's first version scored it at 76/412 with the wrong one.
  Two runs of the same arbitration disagreeing is what said the model and not the
  candidate table was at fault.

## What is established, and what is not

**Established:** the port reproduces the micro-executed routine at every value
measured, with `asserted_semantics` **empty** — no model of this campaign's was
involved, all 92 instructions are the SLEIGH module's rendering of retail's.

**Not established:** that the module's `vmsum4fp128` is the console's. The
summation order above is the module's, the module has been found broken four
times, and nothing here compares it against hardware. That is the campaign's
standing assumption, not a result of this cycle — and this is the place where it
would show if it were wrong, which is worth writing down while it is fresh.

## Not established, also

- Whether modelling the transform kernel's composition in float32 would drive its
  residue to zero. It should; it has not been done.
- `0x82381068`, the cosine of cycle 1410's refused curve, is a **different**
  routine from this one. Whether it is portable by the same method is untested,
  and the curve stays refused until it is.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 77 tests, OK
audit_scalar_sin_cos --check          206 cases, 412 values, 0 failures
audit_scalar_sin_cos --arbitrate      LR/rounded 412/412, next best 298
```

## Next

`0x82381068` — the other cosine. Cycle 1410 refused the layout-1 response curve
because `std::cos` disagreed with it at one argument in 96. It is a 54-instruction
leaf with its own coefficient table at `0x8267A5A8`, so the method that worked
here applies directly: read the table, model the reduction, arbitrate whatever
the ISA does not fix, and sweep. If it ports, the curve stops being refused and
`0x82229250` is complete on both layouts.
