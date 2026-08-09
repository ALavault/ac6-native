# Cycle 1361 — an input command system, not a flight controller

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus was read.
- No product C++ changed, no contract changed.

## The consumers, found by an arithmetic filter rather than a scan

Fourteen functions touch displacement 3660 on a non-stack base — the collision
this thread has met a dozen times. The discriminator is that a genuine reader
must reach the **same object** `0x82211DF8` writes:
`[0x826E4EB4] + 0x20000 + K`.

**Three** functions build exactly that address with exactly that arithmetic, and
each tests **single bits** of the edge word with `rlwinm rX,rX,0,N,N`:

| function | object | slots it tests |
|---|---|---|
| `0x82191AE0` | `context+0x26854` (the third) | 4 |
| `0x82251918` | `context+0x279B8` (the fourth) | 0, 2, 5, 7, 8, 9 |
| `0x82276D68` | `context+0x279B8` (the fourth) | 17 |

The four objects are **not interchangeable**. Different subsystems read different
ones, and each cares about a handful of named slots.

## What the chain is

```
device snapshot -> record -> per-binding deadzone and scale -> active-slot mask
                -> edges and auto-repeat -> "slot 5 was just pressed, do this"
```

That is an **input command system**, with auto-repeat for held slots.

## And it is not the flight controller

Cycle 1358 established that the float on this tick is accumulated into repeat
timers rather than integrated into anything. This cycle finds its consumers
testing **single slot bits to trigger discrete actions**.

Nothing on `0x821CA908` integrates a position or an orientation. The plan's A7
item pointed here for *"le contrôleur de vol"*, and this tick is the command
layer above it.

That is worth saying plainly because five contracted behaviours came out of this
thread and they are all real — `retail_input`, `retail_input_record`,
`retail_input_binding`, `retail_slot_repeat`, `retail_slot_gather` — but they
deliver **the input command path**, not flight. A3.2's integrator is still
unlocated.

## A gap the consumers themselves point at

All three read only the **edge word**. None of them reads the analogue value
arrays the binding layer fills at `this+0xE58` and `this+0xED8`.

So the per-binding deadzone and scale work — the part that looks most like flight
input — has no consumer identified on this tick. Whatever reads those arrays is a
better lead for the flight controller than anything else this thread has turned
up, and it was found by noticing what the consumers do **not** touch.

## Not established

- What the three consumers do with their bits.
- What reads the first two objects.
- What reads the analogue value arrays.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

The analogue value arrays. `retail_input_binding` writes two floats per slot into
`this+0xE58` and `this+0xED8`, and nothing found so far reads them. That is the
one thread here that still points toward a flight model rather than a menu.
