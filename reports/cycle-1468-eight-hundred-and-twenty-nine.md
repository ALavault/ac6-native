# Cycle 1468 — eight hundred and twenty-nine

## Qualification

- **No Ghidra run and no oracle pass.** The product and the map container.
- Product C++: a test only, no library change. ctest **57 → 58**.
- **No contract entry.** The heading, the altitude and the rates are mine; what
  is contracted is the integrator and the heightfield, and both were already.

## The question cycle 1467 left

Do `at68` and the terrain share an origin? `kMidFloor` is **10.0**, from
`0x82003214`, and retail applies it to `at68` and to nothing else — a
**constant**. This map's ground runs 0.00 to 487.44.

Flown: the contracted integrator, level at 60, from the open sea north across
the coast, the city and into the hills, 1800 ticks at 60 Hz.

```
ticks 1800 (off map 0)  at68 60.00  highest ground 171.96
lowest clearance -111.96  clear 971  below 829
```

> **The aircraft ends 829 of 1800 ticks below the ground under it.**

That does not answer the origin question and the test says so. What it does
establish, as a number rather than an argument, is that **the integrator alone
does not keep an aircraft above terrain** — which is exactly consistent with
cycle 1466: contact is answered by `CMapManager` slot 80, a separate query that
returns the nearest hit among parts and terrain. A constant floor and a terrain
query are two different jobs, and retail has both.

## Three wrong runs before that one

**The rates.** `FlightRates` members are `to64`/`to68`/`to72`, not `at64`. A
compile error, and the cheapest of the three.

**The transect.** The first run flew inland from `(-40000, -40000)` and was below
ground for **1800 of 1800** ticks. True, and useless: that line never leaves high
ground, so it cannot show a crossing. A test that can only produce one answer
has not measured anything.

**The scale.** Moved to start over water, the run reported `highest ground 0.00`
— twice. I raised the rate from 900 to 5400 and it reported `0.00` again. The
aircraft was moving **6.94 units a tick, not 25**: I passed `kRateToStep` as
`rate_scale` and the integrator applies `kRateToStep` itself. The run I had sized
for 45,000 units covered 12,500 and never reached land.

I adjusted the numbers twice before printing the trajectory. Printing it took one
minute and showed the answer immediately. *Measure the instrument before trusting
it* applies to a test as much as to a scan, and the instrument here was my own
call.

## Not established

- Whether `at68` and the terrain share an origin. Still open. The evidence
  available is suggestive and not more: retail's floor is 10.0 and this map's sea
  is 0.0, so a floor ten units above sea level is *consistent* with a shared
  origin and equally consistent with a coincidence.
- What `rate_scale` is for, given the integrator already applies `kRateToStep`.
  The header does not say and this cycle did not read it.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 58
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Read `rate_scale`.** It is a parameter of a contracted function that this
cycle misused and the header does not explain, which is the kind of gap that
produces a wrong test rather than a caught one. `integrate_session_position` is
ported from an address; the parameter has a source, and reading it is cheap.
