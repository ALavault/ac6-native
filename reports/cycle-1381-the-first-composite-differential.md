# Cycle 1381 — the first composite differential

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_step_driver.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 37**, was 36.
- New tool `tools/audit_flight_step_driver_microexec.py`, new artefact
  `analysis/flight/flight-step-driver-microexec.tsv`.
- **Contract: the eighteenth behaviour**, `retail_flight_step_driver`.

## The step is the flight model's whole per-frame entry point

`0x82283898` is slot 11 of the base vtable, inherited by `0x8200F270`. Cycle 1379
established that a non-empty slot 11 is what makes a model fly — the sibling
classes override it with the shared empty `blr`.

```
fmr   f31,f1                  its own float, in seconds
lwz   r11,112(r31)
addi  r30,r11,96              r5 = [this+112] + 96
call slot 30 (this)                        0x82302DB0  contracted
call slot 31 (this, f1, r5)                0x82303110  contracted
call slot 32 (this, f1, r5)                0x82302C88  contracted
if bit 1 of [this+332]:
   zero +360, +364, +304, +308, +312
   call slot 33, then 0x82282938, then 0x82326FE8
```

**One float reaches every stage, unchanged.** All three slots it dispatches are
now contracted behaviours.

## The reset is bit 1, and measurement found it before the decode did

Cycles 1371 and 1375 both wrote "bit 0 of `[this+332]`".

The probe ran flags 0, 1, 2 and 3 and counted stubbed calls:

| flags | stubbed calls | the five fields |
|---:|---:|---|
| 0 | 4 | unchanged |
| **1** | **4** | **unchanged** |
| 2 | 5 | all zero |
| 3 | 5 | all zero |

Bit 0 does nothing. `rlwinm r11,r11,31,31,31` is a rotate left by 31 — a rotate
**right by one** — keeping bit 31, which selects **bit 1**.

The measurement came first and the decode confirmed it. That ordering is worth
noting: the same rotate-mask idiom produced cycle 1380's "bit 3 should be bit 4",
and reading it correctly twice in two days was evidently not something to rely
on. Running four flag values costs one Ghidra pass and cannot be misread.

## The first composite differential

```
flight_step_driver_microexec=pass cases=8 values_compared=64
```

Every earlier audit in this campaign measured **one function against one port**.
This one runs the step with the **real** slot 30 reached through a **real virtual
dispatch** — a synthetic vtable holding the genuine addresses — and compares the
object against `apply_flight_step`, which composes the contracted control-surface
port with the step's own reset.

It therefore tests three things no per-function audit can:

- that the dispatch reaches slot 30 at all;
- that the step hands slot 30 **its own float**, unchanged — the case with a
  ten-times-larger step must move the ramp ten times further;
- that the reset runs **after** slot 30, not instead of it.

That last one is observable because the reset spares three of slot 30's eight
outputs. `+368`, `+372` and `+376` keep what slot 30 computed, so a reset that
short-circuited the call would leave the *seed* values there instead. The control
`CONTROL skipping slot 30 must disagree` fires on all 32 sweep points.

The stubbed-call **count** is part of the check: four without the reset, five
with it. That is the evidence that slot 33 runs only on the reset path, and it
would catch a case whose flags were built wrongly.

## What is stubbed, and why it is not a hole

Slots 31 and 32 are stubbed **at their real addresses**, which the synthetic
vtable holds verbatim — so the dispatch under test is the real one even where the
callee is not executed. Both are contracted separately, both take the position
block rather than the model, and both contain VMX128 this instrument cannot run
without the register-file bridge.

Stated in the tool's docstring before anything else, so a reader meets the
limitation before the result.

## Not established

- The composed effect on the **position block**. Slots 31 and 32 write there and
  are stubbed here, so this audit says nothing about it. Closing that needs the
  bridge, and it is the honest next boundary of A3.2.
- Slot 33, `0x82282938` and `0x82326FE8`.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 27 (1351–1371, 1374, 1376–1379) |
| implementation/integration spent on A3.2 | 8 (1354–1356, 1372, 1373, 1375, 1380, 1381) |

**The flight model's per-frame chain is now contracted end to end on the model
side**: the step, and all three virtuals it dispatches.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 18 behaviours
ctest                                 100% passed, 0 failed out of 37
tools/tests                           Ran 77 tests, OK
flight_step_driver_microexec          pass 8 cases, 64 values
```

## Next

The position block. Slots 31 and 32 both write it and both are stubbed in the
composite; the integrator's own differential covers its three components in
isolation. Running the composite with slot 31 **live** needs the VMX128
register-file bridge that cycle 1301 built and that has not been used since — so
the next slice is to measure the bridge against this case, which is a cheap and
well-controlled first use: the expected result is already known from the
integrator's own contracted differential.
