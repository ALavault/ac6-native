# Cycle 1391 — one port for both branches

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_control_blend.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 42**, was 41.
- New tool `tools/audit_control_blend_microexec.py`, new artefact
  `analysis/flight/control-blend-microexec.tsv`.
- **Contract: the twenty-third behaviour**, `retail_control_blend`.

## The bounded search worked

"What reads `+144`, `+148`, `+152`" is **100 functions** unbounded. Filtered to
methods of this class family plus the contracted chain's own callees, it is
**nine** — and two of them are a matched pair of accessors that close the loop.

That is the shape cycle 1378 established and cycle 1384 vindicated: bound the
population *before* scanning, and treat the filter as part of the evidence.

## The loop closes

```
retail_live_flight_axes    writes the control axes    +304, +312
retail_flight_rate_servo   turns them into rates      +144, +152
retail_live_flight_step    decays the stored values   +408, +412
retail_control_blend       blends all three into one clamped value
```

Four contracted behaviours, and the state flows from one to the next. Slots 17
and 18 are the first functions in this thread whose *inputs* are entirely fields
other contracted behaviours produce.

## One port covers both branches

They are slots 17 and 18 of the **base** vtable, inherited unchanged by
`0x8200F270` **and** `0x8200F310`. Everything else in this thread has needed a
separate port per branch; this does not.

They are also the same function twice — 38 instructions each, differing only in
which fields they touch and which constant they scale by. Kept as one
implementation with parameters rather than two copies, so a later divergence
would be a visible edit and not a silent drift.

## What they compute

```
value = axis
if bit 1 of [+332]:
    scaled = rate * scale
    if |scaled| > |axis|:  value = scaled      strict: a tie keeps the axis
    store value                                 BEFORE the clamp
else if |stored| >= 2^-16:
    value = stored + value
clamp to [-1, +1] by two early returns
```

**The store precedes the clamp**, so a case whose result saturates leaves an
unclamped number in memory. The differential's `slot17-reset-mantissas` returns
**1.0** and stores **1.0822536945343018**, and both are compared.

## Two more seven-digit literals

`0x82007F78` = **0.6366198062896729** and `0x82008AD8` = **0.3183099031448364** —
`float32(0.6366198)` and `float32(0.3183099)`, not `float32(2/π)` and
`float32(1/π)`, which are different words.

That is the **third and fourth** in this subsystem, after cycle 1374's
`0.3183099` and cycle 1386's `0.15915495`. The pattern is now firm enough to
state as a rule for this codebase: **a constant near a reciprocal of π is a
decimal literal until compared.**

## Three test bugs, none in the port

The tie control failed three times running, and each failure was the fixture:

1. `axis / scale * scale` does not round-trip, so two of my "ties" were not ties;
2. built forwards instead, the rates reached 20 and every answer was the clamp;
3. only with the rates kept below the clamp did the control read zero.

The port was unchanged throughout. Worth recording because the temptation at step
one was to loosen the assertion — and the port's `>` really is a strict
greater-than, which the direct case `equal_magnitudes_keep_the_axis` had already
established.

## And the address checker caught a fourth mistake

The first contract insertion failed:

```
mission01_native_gate=fail reason=retail_control_blend derivation
  reconstruction/ace-combat-6/include/ac6/retail_control_blend.h
  never cites retail address 0x82069C2C
```

The epsilon's *address* was in the contract's claim and only its *value* was in
the header. That is a citation to nowhere, which is precisely what
`audit_ac6_contract_addresses.py` exists to refuse, and it is the sixth time it
has caught its author. Fixed by adding the address to the header, not by removing
it from the contract.

## The differential

```
control_blend_microexec=pass cases=18 values_compared=36
```

Both functions, nine cases each, the returned value **and** the stored field
compared. Nothing stubbed, nothing capped.

## Not established

- What calls slots 17 and 18. They return a value in `f1` and are accessors, so
  the callers are the consumers of the blended control.
- The second axis. There is no slot for `+308`/`+148`/`+410` in this pair —
  only two of the three axes are blended this way.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 13 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387–1391) |

Six consecutive cycles ending with a contracted behaviour.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 23 behaviours
ctest                                 100% passed, 0 failed out of 42
tools/tests                           Ran 77 tests, OK
control_blend_microexec               pass 18 cases, 36 values
```

## Next

What calls slots 17 and 18 — offsets 68 and 72 in the vtable. The same bounded
dispatch search that found these, run the other way: the callers are the
consumers of the blended control, and they are where the flight model meets
whatever steers by it.
