# Cycle 1413 — two defects under a refusal

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays **51**. **No contract entry** — the port is
  ready and deliberately not shipped this cycle; see *Next*.
- `scripts/MicroExecuteFunction.java` gains two overrides.
  `tools/audit_scalar_cos_microexec.py` and
  `analysis/flight/scalar-cos-microexec.tsv` are new.

## Cycle 1410's refusal is withdrawn

Cycle 1410 refused to port the layout-1 axis response curve at `0x82229470`,
because `std::cos` disagreed with `0x82381068` at one argument in 96 even after
the block's `frsp`. **That measurement was taken against a mis-executed
routine.** The figure is void and the refusal with it.

Finding it took building the port first. The model was derived from the listing
and the table at `0x8267A628`, and it disagreed with the micro-execution at
**163 of 178** values — while agreeing with `math.cos` exactly. A port that is
*more* accurate than the routine it copies is not a port; it is a signal that
one side is being run wrong.

## The first defect: `fctid` truncates

Capturing the routine's own intermediates at a fixed step count, rather than
inferring them:

```
x = 1.4818833271649967   f11 = 3.0526796539598933   (pi/2 + |x|, correct)
                         f0  after fcfid(fctid(...)) = 0.0
```

`(1/π)·3.0527 = 0.9717`, whose nearest integer is **1**. The module returns 0.

A direct control, on values where truncation and rounding visibly separate:

| argument | module | truncate | nearest |
|---:|---:|---:|---:|
| 3.683099 | **3** | 3 | 4 |
| 6.866198 | **6** | 6 | 7 |
| 1.454930 | 1 | 1 | 1 |
| 32.330989 | 32 | 32 | 32 |

`fctid` under `fctidz`'s semantics. And the harness carries **no FPSCR at all** —
`grep -i 'fpscr\|rounding'` over `MicroExecuteFunction.java` returns nothing — so
this is not a rounding mode this script forgot to seed. It is the module.

The consequence for this routine is not subtle: with `n` one short, the reduced
argument lands a whole quadrant outside the sine kernel's range, and the answer
is wrong by up to **5.8e-09**. That is precisely what cycle 1410 measured and
attributed to the routine.

## The second defect: double-precision `fmadd` is not fused

With `fctid` overridden the disagreement fell from 163 to **14**, all one ulp. A
fit said "unfused Horner, 176/176" — and a fit is not a measurement, so:

| `a`, `b`, `c` | module | fused | unfused |
|---|---|---|---|
| 0.1, 0.1, −0.01 | **1.7347e-18** | 9.0206e-19 | 1.7347e-18 |
| 1+2ulp, 1−1ulp, −1 | **0.0** | −4.9304e-32 | 0.0 |

`fmadd` is one rounding in the ISA. The module does two.

## And the reason nothing shipped is affected

The same two controls against **`fmadds`**, the single-precision form, at
`0x82096F04`:

| | module |
|---|---|
| 0.1f, 0.1f, −0.01f | **FUSED** |
| 1+1ulp, 1−1ulp, −1 | **FUSED** |

The corpus has **1,959** single-precision fused multiplies against **109**
double ones, and `fctid` appears **10 times in 6 functions** against 979
`fctidz`/`fctiwz` that name their rounding in the mnemonic.

Cross-referencing the six `fctid` functions against every `retail_addresses`
entry in both contracts: **no contracted behaviour cites one.** Every flight
port uses the single-precision form, which is also why their `std::fmaf` calls
have always agreed.

So the blast radius is: cycle 1410's refusal, and nothing else.

## Both defects are now overridden, and the routine differentiates

`override 0x82381094 fctid` plus ten `fmadd`/`fnmsub` overrides:

```
cases=178 values=178 failures=0
and it is not std::cos: 61 of 178 differ from the library
```

**178 of 178, bit for bit.** These are asserted semantics and labelled as such —
but unlike `vpermwi128` there was nothing to arbitrate: `fmadd` is fused and
`fctid` rounds to nearest even, both stated by the ISA. The control is that a
port derived independently from the listing then agrees at every value, which it
would not do if either assertion were wrong.

The routine really is not `std::cos` — 61 of 178 differ — but the gap is a last-
ulp gap, not the 5.8e-09 the broken instrument reported.

## An open finding this cycle did not cause

`CLAUDE.md` and the plan both cite the harness calibration at **138/138**. It
currently reports **0 of 138**, every case `differs in ['region_dumps']`.

The control: reverting `MicroExecuteFunction.java` to HEAD and re-running gives
**the same 0 of 138**. So it predates this cycle and my two overrides are
exonerated — but the campaign's headline instrument figure does not reproduce,
and that is worth more attention than a line at the end of a report. It is the
first thing to settle next.

`audit_vmx128_behaviours` still passes, so whatever it is, it is not general.

## What this cost, and the rule

Three cycles of work rested on `0x82381068`: 1410 refused a port over it, 1411
counted it among the seams, and this one nearly concluded the routine was
inaccurate. The instrument was never measured against it, because it looked like
ordinary scalar double arithmetic — no vector lanes, no asserted semantics, the
boring case.

**The boring case is where an instrument defect survives longest**, because
nobody instruments it. `fctid` and `fmadd` are more ordinary than `vpermwi128`
and were wrong for longer.

## Not established

- Whether the other five `fctid` functions matter. They are `0x823807E0`,
  `0x823809D8`, `0x82380F98`, `0x823821F0`, `0x823823A0` — a libm cluster, none
  contracted, none read.
- What the real seam between the ported cosine and the response curve is. Cycle
  1410's 1-in-96 is void; the correct figure has not been measured.
- Why the calibration reports 0 of 138.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 29 behaviours
ctest                                 100% passed, 0 failed out of 51
tools/tests                           Ran 77 tests, OK
audit_vmx128_behaviours               pass
audit_scalar_cos --check              178 cases, 178 values, 0 failures
audit_microexec_harness_calibration   0 of 138 -- OPEN, and predates this cycle
```

## Next

**The calibration.** An instrument whose own 138-case baseline does not
reproduce is not one to add behaviours against, and two defects found in one
cycle is a reason to take that seriously rather than route around it. Settle
what changed before contracting the cosine.

Then the cosine port and the response curve, both of which are now one
mechanical cycle: the model is verified at 178 of 178, and `curve_expected` in
`audit_flight_axis_curve_microexec.py` needs only to call it instead of
`std::cos`.
