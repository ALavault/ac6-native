# Cycle 1364 — the angles come from object fields

## Qualification

- Ghidra was used to list call sites and data references. **No oracle pass.**
- No product C++ changed, no contract changed.

## Six call sites, and what each passes

`0x822A2B50` builds the argument block whose `+0x18` and `+0x1C` become two of
the three angles `0x822A1E80` applies. Six callers, none a vtable slot:

| function | f1 | f2 | f3 |
|---|---|---|---|
| `sub_822A2BE8` | `[obj+0x10]` | `[obj+0x14]` | `f31` |
| `sub_822A2C80` | `f3` | `f3` | `f3` |
| `sub_822E9A30` | `[obj+0x14]` | `[obj+0x18]` | — |
| `sub_822EA220` | — | `[obj+0x24]` | — |
| `sub_822EC998` | `f31` | `f31` | `f31` |
| `sub_822ED070` | `f31` | a `.rodata` constant | `f31` |

`sub_822A2C80` is **the unit's own vtable slot `+0x44`** — one of the two
transform entry points cycle 1337 found on `CAce6Unit`. It passes **one incoming
float three times**, so on that path all three rotations take the same angle.

Four of the six take their target as `[[0x826E4EB4] + rN + 1028]` — from the
context, at `+0x404`, indexed by a register.

## The finding

**Not one of the six reads anything from the input command path.** No
`0x826EDB98`, no command frame, no slot mask. The angles come from **object
fields** — `+0x10`/`+0x14`, `+0x14`/`+0x18`, `+0x24` — or from a register the
caller already held.

So something between the input tick and the transform writes those fields, and it
is **neither of the two ends this campaign has mapped**:

- the input tick is five contracted behaviours, and cycle 1363 established every
  float in it is a timer;
- the transform is contracted as `retail_transform`.

The gap is real, bounded and now named at both edges: whatever writes
`obj+0x10`/`+0x14` is the flight model, and it sits between two things that are
already in the product.

## What sixteen cycles of A3.2 have produced

Worth stating plainly, since the target has moved several times:

- **six contracted behaviours** — `retail_transform`, `retail_input_binding`,
  `retail_slot_repeat`, `retail_slot_gather`, plus `retail_input` and
  `retail_input_record` from A7 — each with static evidence, a native test, a
  micro-execution differential and a derivation;
- ctest from 29 to 33;
- two ends of the flight chain mapped, and the gap between them reduced to one
  question about two object fields.

And several corrections that stuck: the transform is not a matrix builder, the
player copies a pose rather than computing one, 27% of the binary's vtables carry
no RTTI, and every float on the input tick is a clock.

## Not established

- What writes `obj+0x10` and `obj+0x14`.
- What the four context-sourced objects are.
- What `sub_822A2C80`'s single float is.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

What writes `obj+0x10` and `obj+0x14` for `sub_822A2BE8`'s object. It is the one
question left between two contracted ends, and the object is reachable: four of
the six callers name it as `context+0x404` indexed, so the population is bounded
by that indexing rather than by a displacement scan.
