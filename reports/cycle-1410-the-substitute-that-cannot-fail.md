# Cycle 1410 — the substitute that cannot fail

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- No product C++ changed; ctest stays **50**. **No contract entry — the port is
  refused**, and the reason is measured below.
- New artefacts `analysis/flight/flight-axis-curve-microexec.tsv` and
  `tools/audit_flight_axis_curve_microexec.py`. New shape in
  `INSTRUMENT_DISCIPLINE.md` (thirty-ninth, indexed).

## The block

Cycle 1409 named the layout-1 response curve at `0x82229470` and left it out of
the port. It is thirty-three straight-line instructions around one call:

```
t = 1.0 - float32(f(|x| * pi/2))     pi/2 at 0x82069E48, f29 = 1.0
if (t > 0.9) t = 1.0                 0.9 at 0x820078C0; fcmpu then ble
x' = fsel(x, t, -t)                  so -0.0 takes +t
```

run for `+2104` first and `+2108` second, both computed before either is stored.

## What `0x82381068` is, established by running it

A 54-instruction leaf: argument reduction by `fctid`, a quadrant parity in
`clrldi r10,r10,63`, a minimax polynomial over a table at `0x8267A5A8`. Nothing
in that shape says whether it is sine or cosine, so it was run at seven angles
where the two diverge:

| | exact matches |
|---|---:|
| `std::cos` | **3 / 7** |
| `std::sin` | 0 / 7 |

**It is cosine.** And the same table is the first warning: it matches the
library at three of seven, not seven of seven.

## The differential that passed and meant nothing

Eight cases of the block itself, entered at `0x82229470` with the callee **live
and unstubbed**. **16 values compared bit for bit, no tolerance, zero
failures** — against an expectation computed with `std::cos`.

That number is worthless. `std::cos` and `0x82381068` are different functions
that differ in the last ulp of the double; the block's `frsp` rounds to float32
and *usually* absorbs it. Eight hand-picked points cannot separate "the port is
right" from "the eight points landed where the rounding saves it".

## The measurement that decides it

Sweep the domain the block can reach — `|x|` in [0, 1] maps to [0, π/2] — and
compare on the **block's output**, which is the quantity anything downstream
sees:

```
96 arguments: 7 split at the cosine, 1 SURVIVES the 0.9 snap
  x = 0.9263  angle = 1.455053448677063
  retail   = 0.8845153450965881
  std::cos = 0.8845154047012329
```

Six of the seven fall where `t > 0.9`, so the snap replaces them with an exact
`1.0` and they are invisible. **One does not.** There is a demonstrated argument
in the block's own domain where a `std::cos` port returns a different float from
retail.

So the curve is **not ported**. Not deferred, not approximated with a note —
refused, on a measurement, the same standard cycles 1111 and 1113 set.

## Which number is the right one, and it is not the bigger one

Measured at the cosine the seam reads 7 of 96 and looks fatal; measured at the
output it reads 1 of 96 and *is* fatal. The first would have justified porting
the polynomial for six cases that do not exist. Both numbers are in the artefact
because the difference between them is the finding.

## Correcting the tool, and myself

The first version of `audit_flight_axis_curve_microexec.py` computed its
expectation with `std::cos`, so it was testing the block and the substitution at
once and reporting one verdict for two claims — in the direction that passes. It
now feeds the differential the callee's **measured** return value, so it tests
only the arithmetic around the call. With that separation:

- the block's arithmetic **is** reproduced: 8 cases, 16 values, 0 failures;
- the substitution **is not** admissible: 1 of 96.

Two claims, two verdicts. That is the thirty-ninth shape.

## A second instrument correction

The first run of the block cases guessed a step budget and exited on a **fault**
after ~190 steps, three callees deep into the rest of the function. A fault-exit
dump is not an assertion about where execution stopped, and the six-field scan
of cycle 1409 was the only reason the values were still right.

The cause: the callee's length **depends on its argument** — 47, 51 and 52 steps
at different angles. So the tool now runs in two passes, measuring the callee at
each of the arguments its cases actually use and setting
`steps = 33 + cost_a + cost_b` exactly. All eight now exit at `step_limit` with
`callee_entries == 2`.

## Not established

- Whether porting `0x82381068` itself is worth a cycle. It is a leaf with a
  readable coefficient table, so it is tractable; whether the curve matters
  enough to pay for it depends on which layout the game runs, which cycle 1409
  left open.
- Whether any other shipped behaviour substitutes a library function for a
  retail routine without this control. **This is the first question of the next
  cycle** and it is a grep, not an investigation.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 28 behaviours
ctest                                 100% passed, 0 failed out of 50
tools/tests                           Ran 77 tests, OK
instrument_discipline_index           pass shapes=30 unindexed=0
```

## Next

Audit the twenty-eight contracted behaviours for the same substitution. The
flight chain calls `std::fmaf`, `std::sqrt`, `std::sin`/`std::cos` through
`XMScalarSinCos`, and `std::asin`/`std::atan2` through the math seams —
`audit_flight_math_seams.py` already sweeps two of those, and cycle 1307
certified `XMScalarSinCos` at fourteen angles, which on today's evidence is
fourteen points and not a domain. The question is which of them have a sweep and
which have a handful of chosen points.
