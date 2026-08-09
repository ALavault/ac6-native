# Cycle 1302 — the bridge, and two defects that hid each other

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The bridge

`alias on` copies each vector-register write to its alias — `vs32+n ↔ vrn`, the
one storage the hardware has and this module splits in two. Only the registers
an instruction actually wrote are copied, taken from its own result objects: a
blind mirror would have to choose a direction and would get it wrong half the
time. 46 copies per run of `0x8209CB70`.

## `0x8209CB70` is sincos, exactly

| θ | `out[r3]` | `sin θ` | `out[r4]` | `cos θ` |
|---:|---|---|---|---|
| 0.00 | 0.000000 | 0.000000 | 1.000000 | 1.000000 |
| 0.25 | 0.247404 | 0.247404 | 0.968912 | 0.968912 |
| 0.50 | 0.479426 | 0.479426 | 0.877583 | 0.877583 |
| 0.75 | 0.681639 | 0.681639 | 0.731689 | 0.731689 |
| 1.00 | 0.841471 | 0.841471 | 0.540302 | 0.540302 |
| 2.00 | 0.909297 | 0.909297 | −0.416147 | −0.416147 |
| 3.00 | 0.141120 | 0.141120 | −0.989993 | −0.989992 |

Seven angles, both outputs, including 2.0 and 3.0 which exercise the argument
reduction the prologue builds — `lis r11,0x124` / `ori r10,r11,0x3f6d` rotated
left 30 by `rlwimi` is `0x40490FDB`, π as float32, sign-preserved.

## The sinc question, settled by measurement

It was suggested that `out[r3]` might be `sinc`. The corrupted values invited it:
they matched `sin(θ)/θ` **exactly at θ = 0 and θ = 1** and nowhere between.

**It is `sin`.** Bridged, `out[r3]` is `sin(θ)` at all seven angles. The sinc
agreement was an artefact of the severance.

**And the suggestion corrected me.** Cycle 1300 argued *"a sine that returns one
at zero is wrong, and no interpretation rescues it"* — `sinc(0) = 1`, so an
interpretation did rescue it, and that sentence was overstated. What made the
question answerable was not the argument on either side but bridging the files
and re-measuring: the closed form of a partially-computed value is not
identifiable, and neither the hypothesis nor my rebuttal deserved to be believed
before the computation was whole.

## The composite moves with its input

Seven cycles of an answer that ignored its argument:

| angles | `+0x90` | `+0xA0` |
|---|---|---|
| `(0,0,0)` | `1.000  1.000  0  0` | `0  1.000  0  0` |
| `(0.25,0.5,0.75)` | `0.642  1.303  0  0` | `−0.598  0.111  0  0` |
| `(π/2,0,0)` | `1.000  1.000  0  0` | `0  0  0  0` |
| `(0,0,1.0)` | `0.540  1.382  0  0` | `−0.841  −0.301  0  0` |

Four inputs, four outputs. For the last, `+0x90[0] = 0.54030 = cos(1)` and
`+0xA0[0] = −0.84147 = −sin(1)` — the first column of a rotation about one axis.

**It is still not a clean matrix and no claim is made that it is.** At zero
angles the result should be the identity and is not: there is an extra `1.0` at
`[0][1]` and a zero where `[2][2]` should be one. Residual corruption remains.

## The two defects were hiding each other

The control that makes this cycle worth more than its result:

| bridge | `vpermwi128` override | `(0,0,1.0)` first row |
|---|---|---|
| off | on | `0 0 1.000 0` — invariant across all inputs (cycle 1298) |
| **on** | **off** | `0 0 0.841 0` — near-zero garbage |
| on | on | `0.540 1.382 0 0` — cos(1), and it moves |

Neither fix alone changes anything. Cycle 1298 overrode a defect it had measured,
watched nothing happen, and concluded — correctly on the evidence — that
`vpermwi128` was "real and not the cause". It was real **and** necessary; the
severance was masking it. Two defects on one dataflow, each making the other
invisible, is a shape worth remembering: *the null result of a correct fix is
evidence about the fix only when nothing else on the path is broken.*

## The twenty-seventh shape

`INSTRUMENT_DISCIPLINE.md` gains *fixtures that inherit the subject's
convention*, indexed and audited (`shapes=18 unindexed=0`). Sixteen green cases
could not see the register-file split because every one of them was seeded and
captured using the naming the instruction's own p-code uses, so none ever
crossed. The cases were not weak; they were all cut from the same template, and
their blind spot was shared rather than averaged out.

## Not established

- What `0x822A1E80` computes. The output now varies and carries recognisable
  terms; it is not yet a matrix anyone should read.
- What the residual corruption is. At least one more defect is on this path.
- Whether the alias split affects registers above `vr31`, which have no AltiVec
  counterpart and are therefore never bridged.
- Whether `0x8209CB70` is correct for negative angles or beyond ±π. Seven
  non-negative angles were tested.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
instrument_discipline_index=pass shapes=18 unindexed=0
```

## Next

Find the residual defect the same way the last one was found: run the composite
at zero angles, where the answer is known — the identity — and bisect to the
first row that is wrong. A known-answer input is worth more than three unknown
ones, and it took seven cycles to start using one.
