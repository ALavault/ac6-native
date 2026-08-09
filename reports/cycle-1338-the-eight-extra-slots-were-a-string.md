# Cycle 1338 — the eight extra slots were a string

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## The correction, to yesterday

Cycle 1337 reported `ACE6::CAce6UnitOtherPlayer` as **31 slots** and flagged it:
*"a class that has more slots than the one it appears to derive from is worth a
second look before anything is built on the hierarchy."* The instinct was right
and the number was wrong.

It has **23**, like its siblings. The eight words past the end are ASCII:

```
"GeneralDataProcess\0\0"  "(isi)\0\0\0"  "i\0\0\0"  <two pointers>  "fogParam"
```

A name, a signature, a function pointer — a **script binding table** parked in
`.rdata` between two vtables.

So the rule cycle 1335 adopted needs its caveat written down, and it now is: the
class map says where the **next** vtable starts and nothing about where **this**
one stops. It is an **upper** bound. The tell is exactly what cycle 1337 noticed,
and one byte dump settles it.

## The transform entry points are not specialised

Slot by slot across all three:

| slot | `CAce6Unit` | `CAce6UnitPlayer` | `CAce6UnitOtherPlayer` |
|---|---|---|---|
| `+0x04` | stub | `0x82266390` | stub |
| `+0x0C` | stub | stub | `0x82266390` |
| `+0x34` | `0x822A2B08` | **same** | **same** |
| `+0x38` | `0x822A1F20` | `0x822A6500` | `0x822A6500` |
| `+0x3C` | stub | `0x822A6710` | `0x822A6710` |
| `+0x44` | `0x822A2C80` | **same** | **same** |

Everything else is identical.

**Both transform entry points — `+0x34` and `+0x44` — are inherited unchanged by
the player and the other-player.** A native port needs **one** implementation of
that path for all three unit kinds, not one per class. That is the same
constraint the shared `RetailTransformKernel` was built for, holding one level
further up, and it is measured rather than hoped for.

## Where the specialisation actually is

`+0x38` and `+0x3C`, and the two derived classes install the **same pair**:
`0x822A6500` and `0x822A6710`. The base has `0x822A1F20` at `+0x38` — the slot
`CAce6Unit`'s own constructor calls at its last instruction — and a stub at
`+0x3C`.

So whatever distinguishes a player-controlled unit from a plain one lives in
those two slots, and both player kinds share it. `+0x04` and `+0x0C` take
`0x82266390` in one class each, which is a second, smaller axis.

## Not established

- What `0x822A6500` and `0x822A6710` do.
- What reads the locator at `+0x10`.
- What writes `unit+0xE0`.
- Whether the class map's 811 vtables have other over-wide entries for the same
  reason. This found one; nothing counted them.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
instrument_discipline_index          pass, 20 shapes, 0 unindexed
tools/tests                          Ran 72 tests, OK
```

## Next

`0x822A6500` and `0x822A6710` — the pair that separates a player unit from a
plain one, in a slot the base constructor calls. Two functions is readable
completely, which is the size this thread has learned to prefer.
