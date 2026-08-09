# Cycle 1398 — a controller on the contracted chain

## Qualification

- **No Ghidra run and no oracle pass.** This cycle ports no retail function.
- **Product C++ changed**: `demo_flight_input.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 47**, was 46.
- **No contract entry**, and none is due.

## The path, and where it stops being retail's

```
controller bytes
  -> build_input_record        0x821CAA50   CONTRACTED
  -> apply_input_binding       0x82211C10   CONTRACTED
  -> [ my conversion ]                      INVENTED
  -> set_flight_command        slots 12/13/14  CONTRACTED
  -> the whole flight chain                 CONTRACTED
  -> an attitude
```

Two contracted stages, one invented link, two more contracted stages. The
invented link is one function, `axis_to_command`, and it is in a file named
`demo_`.

## Why that link is invented and not derived

Cycle 1393 established that slots 12, 13 and 14 take a **target angle** and an
**increment**, and that they are the only writers of the model's commands. It did
**not** establish who calls them or with what — that search was never run.

So the conversion is mine:

```
target    = binding value * a chosen full-scale angle
increment = |binding value| * a chosen rate
```

as is the routing of controller axes to slots. If retail feeds those setters
differently — and it may, since they compare a target against the model's
*current* angle and discard anything within a degree — the stick will feel
different from the game while **every rule downstream stays exact**.

That is a sharp division and the last invented link in the chain. Naming it is
worth more than closing it would be worth guessing at.

## Two contracted details survive all the way to the stick

**Negative zero.** The idle record holds `-0.0`, measured at cycle 1323, and
three ports in this chain compare with `>= 0.0` rather than `signbit` because of
it. The test's `idle()` builds a record with negative zeros rather than positive
ones, so the path is exercised the way retail leaves it.

**The empty optional.** `apply_input_binding` returns nothing when the processed
value is exactly zero, because **retail stores nothing** — cycle 1355 found that
with a differential on its first run. Here that means a stick inside the deadzone
contributes no target and no increment, and
`a_deflection_inside_the_deadzone_commands_nothing` pins it.

Neither is a detail this file invented, and neither would survive a bridge
written from the shape of the API.

## The whole way, in one test

`a_full_snapshot_flies_the_contracted_chain` takes a record, runs it through the
binding layer and my conversion, steps the contracted session six hundred times,
asserts the attitude moved, and draws it. Raw input to pixels, through
twenty-five contracted behaviours and one named invention.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 2 (1395, 1396) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
```

## Next

Close the invented link, or narrow it. **Who calls slots 12, 13 and 14** is a
bounded dispatch search at offsets 48, 52 and 56 — the same search that found the
export functions at cycle 1392 and the blend accessors at 1391, and the same one
that has to be filtered to the class family to mean anything. If it lands, the
last invention in the input path becomes a derivation and the stick feels like
the game's.
