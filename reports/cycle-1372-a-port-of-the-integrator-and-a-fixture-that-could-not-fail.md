# Cycle 1372 — a port of the integrator, and a fixture that could not fail

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus for mnemonics, the
  image for the two float constants.
- **Product C++ changed**: `retail_flight_step.h`, `retail_flight_step.cpp`,
  `retail_flight_step_tests.cpp`, `CMakeLists.txt`. **ctest is 34**, was 33.
- **No contract entry**, deliberately — see below.

## Why this cycle stopped researching

Two searches were run first and both returned the wrong population:

- writers of `[this+112]` — **332 sites**, of which all but a handful are stack
  spills at `112(r1)`;
- callers of the setter's slot — slot 8 at offset 32 is a generic index, **261
  dispatches** across unrelated classes with unrelated signatures.

One thing did come out of the first search, and it is the answer to last cycle's
question: **`sub_822815E8` is two instructions, `stw r4,112(r3); blr`, and it is
slot 8 of all three vtables in the family.** `[this+112]` is set through a
virtual setter. What calls that setter is not established, and chasing it is
another walk down a list of 261.

The plan's instruction is explicit: *"At the end of each gate, produce executable
code and a regression test. Do not continue research merely because more fields
remain unnamed."* A3.2 stood at 21 research cycles against 3 implementation
cycles. So this cycle ports what is fully read.

## The port

`integrate_flight_position(position, rates, rate_scale, mid_bias, step)`, the
tail of `0x82303110` from `0x82303558` to `0x82303694`:

```
s64 = rate_scale * rates.to64        fmuls
s68 = rate_scale * rates.to68        fmuls
s72 = rate_scale * rates.to72        fmuls
s68 = s68 - mid_bias                 fnmsubs -- the middle rate only

at72 += (s72 * step) * (1/3.6)       fmuls then fmadds
at68 += (s68 * step) * (1/3.6)
at64 += (s64 * step) * (1/3.6)

if at68 < 10.0:  at68 = 10.0         and on no other component
```

Both constants are words of the image: `0x82069B40` = **0.2777777910232544**,
`0x82003214` = **10.0**.

Two values are **arguments rather than guesses**, per the A7 rule that unresolved
fields stay explicit rather than delaying a port: `rate_scale` is `[model+32]`,
clamped just above against an unread `f26`; `mid_bias` is the product `f25*f24`
in `fnmsubs f10,f25,f24,f10`, neither factor traced. A port that dropped them
would be a different function; one that invented them would be worse.

## Correcting myself before it shipped

I first wrote the floor as `if (!(at68 >= 10.0f))`. That is wrong on NaN.

`bge cr6` encodes as `bc 4,24,...` — branch when the **LT bit is false**. An
unordered `fcmpu` leaves LT, GT and EQ all clear, so a NaN **takes** the branch
and the store is skipped. `!(x >= f)` is true on NaN and would apply the floor;
`x < f` is false on NaN and does not.

Same shape as the `fsel` negative-zero rule in `retail_input_binding.h`: the
branch condition has to be read off the encoding, not off the mnemonic's English
name. The test `a_nan_takes_the_branch_and_keeps_its_nan` pins it.

## The fixture that could not fail

The suite's first run reported:

```
controls: unfused=0 floor-on-all=1215 double-precision=0
FAIL  CONTROL multiply-then-add must disagree
FAIL  CONTROL double precision must disagree
```

Two of the three controls **agreed with the reference on all 1,215 points**. Not
because the port is wrong — because every value in my sweep was exactly
representable, so fusing changed nothing and double precision changed nothing.
The suite passed its nine behavioural cases and proved nothing about the
arithmetic that the fused forms exist for.

This is the 28th shape — the fixture whose answer is its own input — arriving
from a new direction: not a self-describing fixture, but a **fixture whose domain
cannot express the difference the rule is about**. Only the
`CONTROL ... must disagree` assertions caught it.

The fix is inputs with full mantissas — `0.1F`, `13.7F`, `907.3F` are not exact
in binary, and a position near 4·10³ pushes the product's low bits under the
sum's ulp. After it:

```
controls: unfused=6 floor-on-all=1170 double-precision=5
```

**Six and five out of 1,215.** Small, and worth stating as small: fused and
unfused agree almost everywhere, which is exactly why a sweep of round numbers
finds nothing and why the difference has to be hunted rather than assumed. The
sweep generator is now one function shared by the sweep and the controls, so the
two cannot drift onto different domains.

## Why there is no contract entry

The plan: *"Aucune behaviour de vol au contrat sans différentiel `microexec`,
même au prix d'un blocage."* This behaviour has `static`, `native-test` and
`derivation`. It has **no microexec**, so it does not enter the contract, and the
fourteen behaviours stand unchanged.

That is the same call cycle 1354 made for the binding layer, and the differential
found a real defect in that port on its first run. `0x82303110` is
differentiable: `r3` a flight model, `r5` a position block, `f1` the step — all
synthetic, all buildable, and the harness already builds objects of this shape.

## Not established

- What calls the slot-8 setter, hence what `[this+112]` points at.
- `f26` (the `rate_scale` clamp) and the two factors of `mid_bias`.
- The direction output at `[model+128/132/136]`: it normalises unless all three
  scaled rates fall below an epsilon in `f11`, and `f11` was not read. Not
  ported, and `scaled_rates` is exposed so the two cannot be computed differently
  when it is.
- Which component is which axis. `at68` carries the 10.0 floor, which is what an
  altitude would carry; that reading is written in the header as a reading.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 22 (1351–1372, of which 1372 is half research) |
| implementation/integration spent on A3.2 | 4 (1354–1356, 1372) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 14 behaviours
ctest                                 100% passed, 0 failed out of 34
tools/tests                           Ran 77 tests, OK
contract_addresses                    pass cited=177 supported=177 unsupported=0
contract_derivations                  pass behaviours=32 gaps=0 multiple=0
```

The same 7 pre-existing failures in the superseded `mission01-native-gate.json`,
unchanged.

## Next

The differential, and the fifteenth behaviour with it. `0x82303110` micro-executed
on a synthetic flight model and a synthetic position block, compared component by
component against `integrate_flight_position` — which also settles `f26` and
`mid_bias` by observation instead of by search, because a capsule can read what
the function loads rather than asking who wrote it.
