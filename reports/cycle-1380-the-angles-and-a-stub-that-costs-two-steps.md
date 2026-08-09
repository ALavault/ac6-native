# Cycle 1380 — the angles, and a stub that costs two steps

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_orientation.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 36**, was 35.
- New tool `tools/audit_flight_orientation_microexec.py`, new artefact
  `analysis/flight/flight-orientation-microexec.tsv`.
- **Contract: the seventeenth behaviour**, `retail_flight_orientation`.

## The last unported slot of the class that flies

`0x82302C88` is slot 32 — the third pure virtual of `0x8200F270`, the class
cycle 1379 established is the one with a working step. Its three rotations are
already in the product from A3.1; what was missing is the **angles**, and they
are pure scalar:

```
row 1   ((limit1252 * step) * 1/15) * axis308     clamped to [-limit, +limit]
row 0   (((1/divisor) * limit1248) * step) * axis304   clamped ABOVE only
row 2   ((limit1256 * step) * 2/3)  * axis312     clamped ABOVE only
```

each then multiplied by `π/180` at `0x82069BF4`. The divisor is **7.0**
(`0x82069D1C`).

**The three clamps are not alike.** Row 1 is symmetric; rows 0 and 2 have no
lower bound at all, so a large negative axis is not limited. That is the detail a
tidy port gets wrong, and it is now *measured*: case 4 of the differential feeds
a step of 1000 with negative axes, and retail returns **−12.47 rad** for row 0
where the limit would give −0.087.

## And cycle 1376 said bit 3

`rlwinm r11,r10,28,31,31` is a rotate left by 28 — a rotate **right by four** —
keeping bit 31, which selects **bit 4**. Cycle 1376's report says bit 3. Corrected
in the header rather than left standing in a report nobody re-reads.

## Observing values that are never stored

None of the three angles reaches memory: each goes straight into `f1` and into a
VMX128 rotation. So the usual "run it and diff the object" does not apply, and
the audit uses two different techniques, each with its own guard:

- **Row 0** is clean. `0x82302B78` computes it, calls the rotation, and
  **returns** — so stubbing the rotation leaves the angle in `f1` at the `blr`.
  The whole function runs; nothing is truncated.
- **Rows 1 and 2** are mid-function values in `0x82302C88`, so each case stops
  with `steps` at the instruction that would consume them. That count is **path
  dependent** — the row-1 clamp has three exits of different lengths — so the
  emitter computes it from the same branch decision the expectation makes, and
  the check asserts the number of calls actually reached.

## The stub costs two steps, and the wrong answer said so out loud

Row 2 failed on all eight cases, and retail "returned" **0.016666667, 1000.0 and
100.377** — in each case *exactly the step that case was given*.

That is not a plausible angle; it is `f1` still holding the value `fmr f1,f30`
put there eleven instructions earlier. The window had stopped early.

The cause is in the harness: **stubs are keyed on the callee's entry address**,
not on the call site. The `bl` executes normally and counts one step; then the
stub fires at the callee's first instruction, counts another, and sets `PC = LR`.
Each stubbed call costs **two** steps. Row 1 passes through none, which is why it
was exact from the first run and row 2 was not.

Worth naming what made this cheap: the failure was **categorical, not
numerical**. Three different cases returned three different step values rather
than three wrong angles, and one glance at the column identified the register.
A near-miss would have been read as an arithmetic disagreement and sent me to
re-read the multiplies.

## And a control that could not fail, again

The first test run reported `symmetric-clamp=0`. With a frame-sized step the
row-0 product never gets within two orders of magnitude of `−limit`, so the
control comparing against a symmetric clamp had nothing to disagree with. The
thirty-second shape, third time this session, caught the same way — by the
control asserting that it fires.

## The differential

```
flight_orientation_microexec=pass cases=8 values_compared=23
```

Eight cases across all three rows, including permuted limits so a port that
crossed two limits fails, both saturation directions, and full-mantissa inputs.
The bit-4 case is **reported and not compared**, because `0x822A6400` is stubbed
and the divisor is then undefined — stated in the tool rather than quietly
dropped.

## Not established

- `0x822A6400`, and therefore the scaled divisor.
- The Euler extraction. Both its routines are certified at 0 ulp (cycle 1376),
  but the extraction itself is not ported.
- What steps the `+2224` object.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 27 (1351–1371, 1374, 1376–1379) |
| implementation/integration spent on A3.2 | 7 (1354–1356, 1372, 1373, 1375, 1380) |

**All three pure virtuals of the flying class are now ported, tested and
contracted**: slot 30 the control surfaces, slot 31 the position integrator,
slot 32 the rotation angles.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 17 behaviours
ctest                                 100% passed, 0 failed out of 36
tools/tests                           Ran 77 tests, OK
flight_orientation_microexec          pass 8 cases, 23 values
```

## Next

The step itself. `0x82283898` is 59 instructions, no vector, and calls only
vtable slots — three of which are now contracted. Micro-executing it against
synthetic objects whose slots 30/31/32 are the real functions would close the
loop from one float to a moved, reoriented aircraft, and it is the first thing in
A3.2 that could be a *composite* differential rather than a per-function one.
