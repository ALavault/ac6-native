# Cycle 1407 — the invention deleted

## Qualification

- **No Ghidra run and no oracle pass.** This cycle ports no retail function.
- **Product C++ changed**: `demo_flight_input.h`, `.cpp` and its tests rewritten;
  `retail_flight_session.h`, `.cpp` extended. ctest stays 49.
- **No contract entry** — the session and the demo bridge port nothing.

## The invention is deleted, not reduced

`demo_flight_input.h` invented a conversion from a binding output to a flight
command: a "full-scale angle" and an "increment rate", feeding the virtual
setters. Cycles 1405 and 1406 found retail's actual player path, and it has
**none of those things** — the entity's field goes straight through as an
increment, into an accumulator that clamps.

So the conversion is gone. What remains chosen is smaller **and different in
kind**:

- **before**: the *arithmetic* was mine, so the aeroplane could respond to a
  stick in a way retail never would;
- **now**: only the *wiring* is mine — which controller axis feeds which entity
  field. Every number that reaches the flight model has been through retail's own
  rules, and a wrong choice swaps two axes rather than changing how the aircraft
  flies.

That distinction is the whole value of the last three cycles.

## And the session had a hole

`FlightSessionState` carried `command36/40/44` and fed the axis block. It never
fed `cmd48` or `cmd52` — the two **holds** that `retail_live_flight_ramps` reads
as its targets. Both ramps therefore decayed for ever, whatever the input.

`+48` and `+52` are accumulators exactly like the three axes; cycle 1405
contracted all five together and cycle 1406 showed `0x82227E10` filling them from
`[+2096]` and `[+2100]`. The state now holds a `FlightInputAccumulators` and the
ramps are fed. `the_holds_reach_the_ramps` fails without it.

A hole that no test could see because no test drove a hold — and none drove a
hold because the demo's invented conversion produced none.

## Two interfaces, both available, and a test that tells them apart

The session now has two steps:

```
step_flight_session(state, config, FlightStick,       dt)   the AI's
step_flight_session(state, config, FlightInputFields, dt)   the player's
```

`the_player_interface_drives_the_same_state` drives a **quarter-degree** command
through both. The player path accumulates it; the AI path **discards** it,
because the setters throw away anything within a degree of the model's current
angle — measured at cycle 1393, and now visible as a behavioural difference
between two entry points onto one state.

## The yaw pair, and what it does not claim

Retail's `+44` takes `[+2112] − [+2116]`. Nothing has traced where those two
halves come from. The bridge drives one signed axis split across the pair, which
**reproduces the arithmetic exactly** while claiming nothing about retail's
sources — and `the_yaw_axis_splits_across_the_difference_pair` pins that the
difference is odd in the input.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 8 (1395, 1396, 1399–1404) |
| implementation/integration spent on A3.3 | 5 (1397, 1398, 1405–1407) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 27 behaviours
ctest                                 100% passed, 0 failed out of 49
tools/tests                           Ran 77 tests, OK
```

## Next

Regenerate the demo capture through the contracted path, with a caption that no
longer has to say the conversion is invented — only the wiring, the camera and
the scene. Then `0x82229250`'s other four writes, which would remove the wiring
from that list too.
