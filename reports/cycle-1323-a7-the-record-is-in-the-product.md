# Cycle 1323 — A7: the record is in the product

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No console emulator, no bridge, no game run.
- **Product C++ changed**: `retail_input_record.h`, `retail_input_record.cpp`,
  `retail_input_record_tests.cpp`, and a tenth behaviour in the playable gate.

## A7 exit criteria

| # | criterion | state |
|---|---|---|
| 1 | `r20` from all callers, and which record it selects | **done**, cycle 1320 |
| 2 | port only the qualified semantics of `0x821CAA50` | **done**, this cycle |
| 3 | unresolved fields explicit rather than delaying the port | **done** |
| 4 | differential across the raw domain where it permits | **sampled, 321 vectors** |
| 5 | the exact boundaries `0x07FF/0x0800/0x0801`, `0x4000/0x7FFF/0x8000/0xFFFF` | **done** |
| 6 | four-record routing with distinct sentinels | **done**, cycle 1320 |
| 7 | deterministic input replay | **next**, not this cycle |

On criterion 4 I did not do 65,536 per axis and I will not claim I did. One case
is ~0.23 s of emulation: the full domain is four hours per half, eighteen for the
eight. The sweep is **255 executions** — 108 points on `LY+` and every named
boundary on the other seven — plus 66 button and field cases, 321 vectors in
total. Boundaries are included by construction and never by a decimation stride.

## Three rules the sweep produced rather than confirmed

**The axis slot takes both halves, in order, and only one of them is gated.**

```
slot = -(float(int16(negative)) * float32(1/32767))    unconditional
if int16(positive) > 0: slot = float(int16(positive)) * float32(1/32767)
```

That is why an idle stick reads **negative zero**. A port writing `+0.0F` there
agrees numerically and differs from retail byte for byte — which a replay
checkpoint catches and a screenshot never does.

**Retail multiplies by the reciprocal; it does not divide.** At a half of 513 the
multiply gives `0x3C804100` and the divide gives `0x3C804101`. One ulp, and the
255-point sweep is bit-exact on the multiply at every point. The negative control
below shows this is not a distinction without a difference.

**The `0x800` deadzone does not gate this path.** Cycle 1315 wrote "at or below
it the lane is not written". A half of **1** already stores `1/32767`. The
deadzone belongs to the other normalisation, at `0x821CB244`, which nothing yet
found reaches.

## Two adjacent slots, two different rules

`+0x44` and `+0x48` — flag bits 14 and 15, fed by `device+0x38` and `+0x3A` — do
**not** follow the axis rule. One signed field, scaled, no second half and no
negation, so an idle scalar reads `+0.0` where an idle axis reads `−0.0`. Their
flag bit is gated at exactly 31 and their slot is not gated at all.

Adjacent slots in one record, filled by one function, under two rules. A reading
would very plausibly have merged them.

## The differential runs in `ctest`, and it can fail

321 retail vectors are reduced to one line each in
`analysis/input-path/input-record-vectors.tsv`, so the test needs no Ghidra.
`tools/emit_input_record_vectors.py` regenerates it from the two sweep workdirs.

**The test was measured before it was trusted.** Replacing the multiply with a
divide in the port — one character — turns four of the 321 red, at exactly the
inputs where the two differ:

```
FAIL axis-LYp-0201:        slot 0x50 got 0x3c804101 want 0x3c804100
FAIL btn-field-dev38-pos:  slot 0x44 got 0x3f1c4139 want 0x3f1c4138
FAIL btn-field-dev3a-pos:  slot 0x48 got 0x3f1c4139 want 0x3f1c4138
```

A differential that cannot fail is a decoration. This one has power against the
smallest error the port can make.

## What is in the product and not interpreted

Named in the header rather than omitted, so nothing is silently invented:

- the constant **3** at `record+0x98`, written by every run, meaning unknown —
  reproduced by `encode_input_record` because a differential over the whole
  record must account for it;
- `+0x8C..+0x94`, zero in every run and **outside** the cleared range, so a
  non-zero value in some other state is not ruled out;
- the second normalisation at `0x821CB244`.

The XInput labels on the flag enum are **labels**. The runs set a bit, not a
button; nothing here measured which physical control a bit belongs to.

## The two estimates, from now on

| kind | cycles | which |
|---|---:|---|
| shared instrument (**not** an A7 cost) | 17 | 1294–1306, 1314, 1317, 1319, 1321 |
| A7 research | 12 | 1307–1313, 1315, 1316, 1318, 1320, 1322 |
| A7 implementation / integration | 2 | the `retail_input` port, and this cycle |

The instrument row is infrastructure and is now largely spent: the harness,
five asserted VMX128 behaviours, the register-file bridge with controls, the
second poison pass, and four pinned module defects are reusable by A3 and A5.
The implementation row is the one that should be read as the rate.

## Not established

- Deterministic input replay — criterion 7, and the next cycle.
- Whether the scalar flag threshold is an integer compare against 30 or a float
  compare against a constant in `(0.000916, 0.000946]`. Both fit every measured
  value and I am not picking one.
- What `device+0x38` and `+0x3A` are. Placed, measured, unnamed.
- Whether a retail service ever presents a second connected controller here.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 10 behaviours
ctest                                100% passed, 0 failed out of 29
contract_addresses                   pass, 155 cited, 155 supported
contract_derivations                 pass, 28 behaviours, 0 gaps
tools/tests                          Ran 72 tests, OK
contract_artifacts (live contracts)  pass
```

## Next

Criterion 7: deterministic input replay, immediately, as instructed — record the
snapshot stream, replay it, and require the record sequence to compare equal.
Then A3.1, the shared transform kernel, under the five sentinel capsules and the
architectural comparison, with **one** `RetailTransformKernel` for flight
orientation and rendered-unit orientation.
