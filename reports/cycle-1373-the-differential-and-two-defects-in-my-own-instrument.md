# Cycle 1373 — the differential, and two defects in my own instrument

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass.** No game ran; this is P-code micro-execution.
- New tool `tools/audit_flight_step_microexec.py`, new artefact
  `analysis/flight/flight-step-microexec.tsv`.
- **Contract: the fifteenth behaviour**, `retail_flight_step`.

## The result

```
flight_step_microexec=pass cases=10 passed=10 values_compared=30
```

Ten cases, thirty float comparisons, **bit for bit** against
`ac6::retail::integrate_flight_position`. The Python oracle is written a second
time from the listing rather than calling the C++, so the two sides are
independent code.

## Where it runs, and the two things it deliberately does not claim

Execution starts at **`0x82303558`**, not the function head. `0x82303110` is 359
instructions and the port documents its tail; entering at the top would run a
prologue and a stretch this campaign has not read. The smaller claim is the one
the port actually makes.

And the VMX normalise at `0x823035EC..0x82303660` is **never exercised**. It is
guarded by `bne cr6,0x82303670` when all three scaled components fall below the
epsilon in `f11`, so seeding `f11 = 1e30` makes every run pure scalar — which
cycle 1306 named as exactly what this instrument can certify. The port does not
implement the normalise either. This audit therefore says nothing about the
direction output at `[model+128/132/136]`, and the tool's docstring says so
first.

## Defect one: a 32-bit constant in a 64-bit register

The first run: `exit=fault`, `callee_entries=8`, and the position **unchanged**.

PowerPC FPRs are 64 bits. The harness's bare-hex seed writes the token verbatim,
so `fpr f12 0x415B3333` — the single-precision pattern for 13.7 — landed as the
double `0x00000000415B3333`, a denormal near **5.4e-315**. Every comparison went
the wrong way, the vector block ran instead of being skipped, and nothing was
stored.

The harness has had the right form all along: `f:` parses a decimal and calls
`doubleToRawLongBits`. This is the thirty-first shape again — *a repository tool
already answers the question* — from the seeding side rather than the reading
side.

## Defect two: my own guard, firing correctly

Second run: `exit=step_limit`, but **`callee_entries=1`** on eight of ten cases.
`steps 50` overshot the window into the epilogue's `bl 0x82384478`.

The count is arithmetic: `0x82303558..0x823035E8` is 37 instructions, the branch
lands on `0x82303670`, and `0x82303670..0x82303690` is 9 more — **46 at most**,
one fewer when the floor is not applied. `steps 46` covers the window and stops
short of the epilogue.

Worth stating plainly: **the assertion `callee_entries == 0` is why this was
seen.** Without it the runs would have passed with values that happened to be
correct, and the audit would have been silently executing four instructions it
had no business executing.

## Defect three, and it is the interesting one: the two sides used different inputs

Third run, and it *failed* — two mismatches, both one ulp:

```
flight-step-2: +68: retail 4115.916015625      port 4115.91650390625
               +72: retail 0.008657408878207207 port 0.008657407946884632
flight-step-9: +72: retail -9.298583984375      port -9.298583030700684
```

Only the cases with long decimals, which is the signature of a rounding
disagreement — so I built all four candidate models and ran them together:
single-rounded or double intermediates, fused or unfused.

**All four reproduced retail exactly.** The rule was never in question.

The spec seeds the emulator with `repr(f32(value))` — the single-precision
number — while a Python literal like `13.7` is a **double**. The oracle was
computing from `13.7` and the emulator from `13.699999809265137`. Two sides
agreeing on the arithmetic and disagreeing on the inputs.

That is a distinct shape from the twenty-eighth (a fixture whose answer is its
own input) and from the thirty-second (a domain that cannot express the
difference): here both sides were right and the *fixture* was inconsistent with
itself. Rounding every input in `expected()` fixed it, and 10/10 followed.

Had I reached for the rule first — "retail must not be fusing" — I would have
un-fused the port to make a green suite, and shipped a wrong one. Building the
four models in one pass instead of one at a time is what made that cheap.

## The fifteenth behaviour

`retail_flight_step` enters `mission01-playable-gate-v1.json` with all four
evidence kinds. Cycle 1372 withheld it for want of exactly this, which is the
second time the rule has cost a cycle and the second time it has paid: 1355
found a real defect in the binding-layer port on its first differential, and this
one found two defects in the instrument and cleared the port.

## Not established

- The direction output at `[model+128/132/136]`, and `f11`.
- `f26`, the clamp on `[model+32]` just above the window.
- The two factors of `mid_bias` (`f25`, `f24`), still arguments.
- What calls the slot-8 setter, hence what `[model+112]` points at.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 22 (1351–1371) |
| implementation/integration spent on A3.2 | 5 (1354–1356, 1372, 1373) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 15 behaviours
ctest                                 100% passed, 0 failed out of 34
tools/tests                           Ran 77 tests, OK
flight_step_microexec                 pass cases=10 passed=10 values_compared=30
```

## Next

`mid_bias` and `f26` are now cheap: a capsule can be extended **upward** from
`0x82303558` toward the function head one block at a time, reading what each
register is loaded from rather than searching for who wrote it. That is the same
move that turned the integrator from a signature match into a contracted
behaviour, and it applies to the other two pure virtuals — slot 30 at
`0x82302DB0` and slot 32 at `0x82302C88` — which the step calls with the same
float and the same pointer.
