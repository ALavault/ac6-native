# Cycle 1300 — cos is right, and the other output is not sin

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The premise test, and it passes

Cycle 1299 said to stop testing the machinery and test the premise: read the
sin/cos results for **all three** calls, not the last one, and check each against
the angle that call was given.

Making the stack a poison region and sweeping the step limit reads the callee
frame at nine points. Each call overwrites the previous one's slots, so the
three plateaus are the three calls:

| steps | `+0x50` | `+0x54` | call | angle | `cos(angle)` |
|---:|---|---|---|---|---|
| 140–260 | 0.979297 | **0.877582** | `0x820A9B30` | 0.5 | 0.877583 |
| 320–440 | 0.997398 | **0.968912** | `0x820A99F8` | 0.25 | 0.968912 |
| 500–547 | 0.931156 | **0.731686** | `0x82211828` | 0.75 | 0.731689 |

`+0x54` is `cos(θ)` to within a few ULP — `-5.5e-07`, `-4.2e-07`, `-2.9e-06`,
the residue of a polynomial approximation — **and each call's value matches that
call's own argument**, routed through the caller's `fmr f1,f2` / `fmr f1,f31` /
`fmr f1,f30` chain.

So the arguments reach the arithmetic, per call, and the arithmetic varies with
them. Six cycles of eliminations and this is the first thing the thread has been
able to state positively.

**Correction to cycle 1296.** It read `3f3b4fcd` on the frame and called it
"the callee's" cos. The three calls go to **three different functions**, and that
value belongs to the third, `0x82211828` — not to `0x820A9B30`, which is what
the surrounding text implied.

## And the other output is not sin

The slot at `+0x50` is negated into `+0x5C` by `820a9b84 fneg f0,f0`, beside cos
— the `-sin` position of a rotation matrix. So the code itself says `+0x50`
should be `sin(θ)`. It is not: `0.979297` where `sin(0.5) = 0.479426`.

Micro-executing `0x8209CB70` **directly**, with no caller, at five angles:

| θ | out[r3] | out[r4] | `sin θ` | `cos θ` |
|---:|---|---|---|---|
| 0.00 | **1.000000** | 1.000000 | 0.000000 | 1.000000 |
| 0.25 | 0.997398 | 0.968912 | 0.247404 | 0.968912 |
| 0.50 | 0.979297 | 0.877582 | 0.479426 | 0.877583 |
| 0.75 | 0.931156 | 0.731686 | 0.681639 | 0.731689 |
| 1.00 | **0.841468** | 0.540278 | 0.841471 | 0.540302 |

Two rows settle it without any curve fitting:

- at **θ = 0** the first output is `1.0`, and `sin(0)` is `0`. A sine that
  returns one at zero is wrong, and no interpretation rescues it.
- at **θ = 1.0** it is `0.841468` against `sin(1.0) = 0.841471` — right, to the
  same few ULP as cos.

`out[r4]` is `cos(θ)` at all five.

The direct call reproduces the in-context values exactly — `0.979297` /
`0.877582` for `0.5` — which is the control that the calling convention I
assumed (`r3`, `r4` as out-pointers, `f1` the angle) is the one the caller uses,
and that the routine is deterministic.

## What is deliberately not concluded

The four non-zero values invite a closed form. `acos(out[r3])` divided by
`θ^1.5` is nearly constant at 0.575 across all four, which is a three-parameter
fit on four points and is exactly the kind of rule cycles 1111 and 1113 were
killed for. **No name is given to what `out[r3]` computes.** The measured values
are recorded and that is all.

Nor is it established *why*. Two readings remain open and this cycle does not
choose between them:

- the emulator mis-executes something inside `0x8209CB70` — but the same
  92 instructions produce a correct `cos`, from what is almost certainly the
  same polynomial machinery, which makes a general execution fault unlikely;
- the routine's first output is not sine at all, and both my reading and the
  caller's `fneg`-beside-cos are wrong about it.

## Not established

- What `out[r3]` is.
- Why the object write is input-independent. A wrong sine still varies with θ,
  so this cycle's finding does **not** explain the invariance — that remains
  open after six cycles.
- What `0x822A1E80` computes.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
```

## Next

Read `0x8209CB70` — 92 steps, small enough to walk instruction by instruction
with a vector capture, the way the callee was bisected in cycle 1297. The
question is narrow and answerable: which instruction produces `out[r3]`, and does
its input already carry the wrong value or does that instruction create it.
