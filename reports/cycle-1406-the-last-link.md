# Cycle 1406 — the last link

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_input_apply.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 49**, was 48.
- New artefact `analysis/flight/flight-input-apply-microexec.tsv`.
- **Contract: the twenty-seventh behaviour**, `retail_flight_input_apply`.

## The chain is contracted end to end

```
build_input_record        0x821CAA50   retail_input_record
apply_input_binding       0x82211C10   retail_input_binding
[ the copy at 0x82229250 ]             not ported
THIS                      0x82227E10   the five increments
accumulate_flight_input   five funcs   retail_flight_input_accumulators
the flight chain                       twenty-two behaviours
```

Controller bytes to attitude, and the only step still unported is one copy.

## Five loads and one subtraction

```
[+2096]            -> +48   a hold
[+2100]            -> +52   a hold
[+2104]            -> +36   an axis
[+2112] - [+2116]  -> +44   an axis
[+2108]            -> +40   an axis
```

Each field is passed **straight through** as the increment. No scaling, no rate,
no full-scale angle — the demo's invented conversion supposed all three and
retail has none of them.

**The one piece of arithmetic is the difference for `+44`**, and it is exactly
what a port written from the shape would lose: four pass-throughs and one
subtraction do not look like a rule, so they get tidied into five
pass-throughs. The control `CONTROL a pass-through for +44 must disagree` fires
on 32 of 33 sweep points, and the test `equal fields cancel` says what a tidied
port would produce instead.

The call order is 48, 52, 36, 44, 40 — not field order, not axis order. It is not
observable here because the five fields are distinct, and it is preserved anyway:
*not observable today* is not *safe to reorder*.

## The differential, and the guard that saved it

```
flight_input_apply_microexec=pass cases=5 values_compared=25
```

with **all five accumulators running live** and nothing stubbed — the second
fully-live composite of the campaign, after cycle 1392's.

The first run used `steps 24` and produced garbage: `callee_entries=2`, so two of
the five calls had run and three had not, and one case reported an accumulator
holding **2.0** — a value the clamp exists to prevent. Twelve mismatches, none
of them the port's.

The check now asserts `callee_entries == 5`. That is the same lesson as cycle
1373's `callee_entries == 0` and cycle 1389's stubbed-call count: **when a
technique stops execution by counting, the count is part of the claim, and
something must fail loudly when it is wrong.** Three cycles have now been saved
by that rule and none has been lost to it.

## What is still not established

The copy at `0x82229250` — what fills `+2096`, `+2100`, `+2112` and `+2116`.
Cycle 1404 established that it writes `+2104` and `+2108` from the contracted
binding layer's first two outputs; the other four fields it writes on paths this
cycle did not read.

So the *shape* of the whole path is contracted and two of its six inputs are
traced to the controller. The other four are not, and the headers say so.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 8 (1395, 1396, 1399–1404) |
| implementation/integration spent on A3.3 | 4 (1397, 1398, 1405, 1406) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 27 behaviours
ctest                                 100% passed, 0 failed out of 49
tools/tests                           Ran 77 tests, OK
```

## Next

Rewire `demo_flight_input.h` onto `apply_flight_input` and delete its invented
conversion, so the demo's controller path is contracted except for the copy. Then
`0x82229250`'s other four writes, which are the last unread step between a
controller and the aeroplane.
