# Cycle 1411 — three seams, swept

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays **50**. No behaviour added — one
  behaviour's evidence and derivation **sharpened**, contract stays 28.
- `analysis/flight/flight-math-seams.tsv` rewritten and now **cited**;
  `tools/audit_flight_math_seams.py` widened.

## The question cycle 1410 left

Cycle 1410 refused a port because `std::cos` disagreed with retail's own cosine
at one argument in 96 — a disagreement eight chosen points had not found and
could not have. Its closing question was whether any **shipped** behaviour
carries the same substitution without a sweep.

A grep over the product separates three kinds:

| what the product calls | verdict |
|---|---|
| `std::fabs`, `std::fmaf` | exactly specified by IEEE 754 — not substitutions |
| `std::asin`, `std::atan2` via `audit_flight_math_seams.py` | swept — **but at 42 chosen points** |
| `std::sin`, `std::cos` in `retail_transform.cpp` | **unswept**, 7 angles, and every rotation in the chain goes through it |

And the tool that guarded the first two **passed at "within 2 ulp"** under a
contract claim of "identical at 0 ulp". A gate looser than the claim it guards
is not a gate. That is fixed here too.

## Two seams promoted from chosen points to a domain

| seam | retail | arguments | worst |
|---|---|---:|---:|
| `std::asin` | `0x82380570` | **212** | **0 ulp** |
| guarded `std::atan2` | `0x820936E8` | **214** | **0 ulp** |

Both are **identical**, now over their whole domains and not at twenty
hand-picked values. The chosen points were kept beside the sweeps rather than
replaced — the endpoints and the `2⁻¹⁶` guard edges are where an approximation
breaks and a uniform sweep steps straight over them.

## And the third is not

| seam | retail | values | identical | ≤2 ulp | worse |
|---|---|---:|---:|---:|---:|
| `std::sin`/`std::cos` | `0x8209CB70` | **412** | 170 | 182 | **60** |

Worst absolute error **3.20e-07** (sine, near −π) and **2.98e-07** (cosine). The
ulp figures reach the millions and mean nothing: they all sit where the true
value is near zero, so the metric exaggerates. The absolute error is the figure
that counts and it is recorded as the baseline.

`retail_transform.h` said "about 1e-06" from seven angles. That was honest and it
was a guess with a number attached. It now carries the measurement, and the
contract's derivation claim carries it too.

**This is the largest fidelity gap left in the scalar flight chain**, and it is
the one seam of three that is real — which is worth stating plainly, because the
cheap conclusion from cycle 1410 would have been "library substitutions are
generally unsafe" and two of the three are exact over 426 arguments.

## The instrument caught me repeating a fixed bug

The first sweep of `0x8209CB70` returned `1.0` for sine at angle 0, `20.09` near
π, and asymmetry between ±1.0. I was one step from concluding the routine is not
sine.

It is **cycle 1300's table, to the digit** — `0.841468` at 1.0, `0.979297` and
`0.877582` at 0.5, `1.0` at zero. Cycle 1302 fixed it: the routine needs
`alias on`, the register-file bridge that copies each vector write to its alias.
My spec did not say so.

That is the thirty-first shape for the fifth time this campaign — a repository
that already answers the question, not consulted — and the sharpest instance yet,
because the *symptom* was the title of an existing report:
`cycle-1300-cos-is-right-and-the-other-output-is-not-sin.md`. The spec template
now carries the reason in a comment, so the next person to write one reads why
before they need to.

Reproducing a known-broken measurement exactly is also the strongest available
evidence that the missing directive is the cause and the routine is not at fault.
That is worth more than the correction.

## The tool no longer tolerates, it declares

```python
SUBSTITUTABLE = {"asin": True, "atan2": True, "sincos": False}
```

`True` means the library call is admissible **only at 0 ulp over the sweep**;
anything else fails. `False` means the substitution is known unfaithful: the tool
does not fail on it — a gate that always fails is not read — but it asserts the
gap has not grown past `SINCOS_BASELINE_ABS = 3.3e-07` and **prints the number
every run**. A figure in the output is harder to ignore than a comment in a
header, which is where this one had been sitting.

```
asin   cases=212 worst=0 ulp   -> identical
atan2  cases=214 worst=0 ulp   -> identical
sincos cases=412 worst=14697170 ulp   -> [DECLARED UNFAITHFUL]
sincos worst absolute error 3.20375e-07 (baseline 3.3e-07)
```

## Not established

- Whether `0x8209CB70` should be ported. It is 92 instructions, a leaf, no calls,
  and now differentially testable at 412 values — so it is tractable. Whether it
  is *worth* a cycle depends on whether anything downstream is sensitive to
  3e-07, and nothing has measured that.
- Whether the vector paths carry substitutions of the same kind. This cycle
  swept the scalar seams only; the estimate instructions are a separate and
  already-refused matter.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 28 behaviours
ctest                                 100% passed, 0 failed out of 50
tools/tests                           Ran 77 tests, OK
contract_addresses                    pass cited=274 supported=274
audit_flight_math_seams               pass 632 cases, 838 values
```

## Next

**Port `0x8209CB70`.** The case for it is now concrete rather than aesthetic: it
is the only unfaithful scalar seam in the chain, it is a 92-instruction leaf with
no calls, the differential already exists and covers 412 values, and closing it
would make every rotation in the flight chain bit-exact instead of
3e-07-accurate. The argument reduction is the interesting part and `alias on`
says it goes through the vector unit, so bound the vector footprint first — that
is what made cycle 1375 cheap and what cycle 1382 was punished for skipping.
