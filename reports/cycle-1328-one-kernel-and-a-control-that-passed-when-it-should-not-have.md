# Cycle 1328 — one kernel, and a control that passed when it should not have

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.**
- **Product C++ changed**: `retail_transform.h`, `retail_transform.cpp`,
  `retail_transform_tests.cpp`, and an eleventh behaviour in the playable gate.

## One kernel, and it is a constraint

`ac6::retail::retail_transform` is the single implementation, and the header says
why in the file rather than in a report: `0x822A1E80` sits on both the flight
path and the rendered-unit path, and two separate ports would be two chances to
pick a different row-or-column convention. Each would be self-consistent, so no
single test would catch the disagreement. There is one kernel and callers use it.

What it carries:

```
identity_basis()                     the three .rodata constants, read
rotate_basis(basis, kept, sign, pair) the arithmetic, exact
rotate_820A9B30 / 820A99F8 / 82211828 keeps row 1 sign -1 / row 0 +1 / row 2 +1
assemble_basis(angle1, angle2, angle3)
```

`assemble_basis` applies **angle2 first**, then angle1, then angle3. The
parameter names follow the retail argument registers and the awkwardness is
deliberate: any friendlier naming would hide the order.

The rows are **not** called pitch, yaw and roll. Nothing measured which physical
axis a row is.

## The control that passed, and should not have

Two negative controls were run against the differential.

Flipping the middle rotation's sign: **24 failures.** Good.

Swapping the composition order — `angle1` applied before `angle2`: **it passed.**

That is a hole, not a success. Every `assemble` vector was at zero angles, where
the order cannot matter, and the twelve rotation vectors each exercise one
rotation alone. The differential had thirteen cases and **no power at all**
against the one property the header makes most of.

Fixed by measuring, not by arguing: a fourteenth capsule, `assemble-mixed`, runs
`0x822A1E80` at three distinct non-zero angles (0.25, −0.5, 0.75) so the order is
visible in the result. The same swap now gives **8 failures**.

A differential is only as strong as the property it can distinguish, and the way
to find out is to break the port on purpose. This one had to be broken twice
before it was worth having.

## Two tolerances, and the difference is the point

The rotation arithmetic is **exact**, and `rotate_basis` takes the cosine/sine
pair rather than an angle so a test can check it without the trigonometry. It is
checked against a pair that is not a real cosine/sine — `(2.0, 3.0)` — so the
test cannot pass by a trigonometric identity, and it checks the aliasing hazard:
both source rows must be read before either is written, exactly as the retail
sites rotate in place.

Every **zero-angle** vector is required to match **bit for bit**, because neither
side approximates anything there. Only the turned cases carry a tolerance, and
the tolerance exists for exactly one reason, named in the header, the test, and
the contract claim:

**`0x8209CB70` is `XMScalarSinCos` and it is not ported.** Micro-execution
reproduces it exactly at seven angles including its argument reduction; the
*product* uses the host library. So the native kernel agrees with retail to about
1e-06 and not to the bit. `retail_sin_cos` is a seam so that porting it later
changes one function.

A replay required to match retail over many frames will need that port. A single
frame does not notice. That is stated rather than discovered later.

## Not established

- `XMScalarSinCos` as native code. The tolerance is entirely this.
- The object's first 16 bytes. `0x822A1E80` never writes them, and the sentinel
  `(17, 29, 43, 61)` came back untouched in all fourteen runs.
- The algebraic composition as a matrix product. Each call transforms the rows in
  place; writing it as a product needs a row-versus-column convention this
  campaign has not measured, and the header does not pretend otherwise.
- **No caller uses the kernel yet.** It is qualified and shared by construction,
  not yet wired into flight or into the renderer.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
contract_addresses                   pass, 163 cited, 163 supported
contract_derivations                 pass, 29 behaviours, 0 gaps
tools/tests                          Ran 72 tests, OK
transform_kernel                     pass, 12/12 rotation cases
```

## The two estimates

| kind | cycles | delta |
|---|---:|---|
| shared instrument (not a slice cost) | 19 | +2 (1326 immediate decode, 1327 `dump`) |
| A7 research / implementation | 12 / 3 | — |
| A3.1 research | 3 | 1325, 1326, 1327 |
| A3.1 implementation | 1 | this cycle |

A3.1 took four cycles from "twenty cycles of hunting a VMX128 defect" to a
contracted native kernel, and three of those four were paid for by the instrument
work that preceded them.

## Next

A3.2, the flight controller: what reads this basis, and what drives the three
angles. `0x82211DF8` and the float it receives is the named lead, and the ladder
does not name that float — so it will not be called delta time here until
something measures it.
