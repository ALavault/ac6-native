# Cycle 1405 — the player's interface

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_input_accumulators.h`, `.cpp`, its
  tests, `CMakeLists.txt`. **ctest is 48**, was 47.
- New artefact `analysis/flight/flight-input-accumulators-microexec.tsv`.
- **Contract: the twenty-sixth behaviour**, `retail_flight_input_accumulators`.

## Six cycles of search had the wrong premise

Cycles 1399–1404 looked for whatever turns a controller into a flight command,
and looked for it as **a caller of the virtual setters** at slots 12, 13 and 14.
Five refutations, no caller.

The premise was wrong. **The input path does not use virtual dispatch at all.**

```
0x82234040   the entity's per-frame function
  -> 0x82229250   copies the contracted binding layer's output arrays from
                  [player+0xE58…] into entity+2104…+2116
    -> 0x82227E10 reads those and calls five functions DIRECTLY on the live
                  flight model at entity+4912
```

Only two functions in ~8,000 touch the binding layer's output arrays, and only
two read `entity+2104…+2116`. The chain was three rare fields long, and every
dispatch search was looking at the wrong kind of edge.

## The five

Thirteen instructions each, identical but for the field and the lower bound:

```
f0 = increment + [field]
[field] = f0                       stored BEFORE the clamp
if f0 < lower:      [field] = lower
else if f0 > 1.0:   [field] = 1.0
```

| function | field | lower |
|---|---:|---:|
| `0x82281FB0` | **+36** | −1.0 |
| `0x82282020` | **+40** | −1.0 |
| `0x82281FE8` | **+44** | −1.0 |
| `0x82281F40` | +48 | **0.0** |
| `0x82281F78` | +52 | **0.0** |

`+36`, `+40`, `+44` are the three commands `retail_live_flight_axes` reads;
`+48` and `+52` are the two holds `retail_live_flight_ramps` reads. **Between
them, these five are every input the contracted slot 30 consumes.**

The two lower bounds are the tell: signed axes clamp to −1, holds clamp to 0.

## Two interfaces onto the same three fields

The virtual setters contracted at cycle 1393 take a **target angle** and discard
anything within a degree of where the model already points. These take an
**increment** and clamp. They are different interfaces, and cycle 1400 already
showed the setters' callers commanding `target = 0.0` — levelling.

So: **the setters are the AI's interface and these are the player's**, and the
campaign found the AI's first and spent six cycles assuming it was the only one.

Nothing about cycle 1393 is wrong; what was wrong was reading "the only writers
of the commands **in the class family**" as "the only writers".

## The differential

```
flight_input_accumulators_microexec=pass cases=40 values_compared=40
```

Five functions × eight cases, passing on the first run: both directions, both
clamps for both bounds, exactly ±1 passing through, negative zero, and full
mantissas.

## And the demo's invented link is closed

`demo_flight_input.h` said its conversion from a binding output to a command was
mine, and that if retail fed the model differently the stick would feel different
while everything downstream stayed exact. It does feed it differently — no target
angle, no increment rate, just **add and clamp** — and that is now a contracted
behaviour rather than a choice.

Rewiring the demo onto it is the next cycle's work.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 8 (1395, 1396, 1399–1404) |
| implementation/integration spent on A3.3 | 3 (1397, 1398, 1405) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 26 behaviours
ctest                                 100% passed, 0 failed out of 48
tools/tests                           Ran 77 tests, OK
```

## Next

Rewire `demo_flight_input.h` onto the contracted accumulators and delete its
invented conversion. Then `0x82227E10` — 174 instructions, the function that
decides *what increment* each accumulator gets from the copied input, which is
the last unported step between a controller and the aeroplane.
