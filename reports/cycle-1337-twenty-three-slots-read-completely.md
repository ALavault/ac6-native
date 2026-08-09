# Cycle 1337 — twenty-three slots, read completely

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## The population was enumerated, not filtered

`ACE6::CAce6Unit`'s vtable is `0x82056874`, and its extent comes from the class
map — the next named vtable is `CAce6UnitPlayer` at `0x820568D4`, whose COL sits
four bytes before it, so **23 slots**. That rule is two cycles old and it was
bought with a wrong answer.

Of the 23, **sixteen are stubs**: `0x822663A8` nine times — the `li r3,0 ; blr`
that `CLAUDE.md` records in 27 of the 811 named vtables — and `0x822DDBE8` seven
times. **Seven are real methods**, and all seven were read.

That is the first complete enumeration in this thread. Cycles 1333 and 1334 both
ended with filtered candidate lists; this one ends with a population.

## Two of the seven reach the transform, and they differ in one thing

| slot | function | reaches |
|---|---|---|
| `+0x34` | `0x822A2B08` | `0x822A23D8`, the 460-instruction builder |
| `+0x44` | `0x822A2C80` (164 instructions) | `0x822A2B50`, the argument-block wrapper |

The other five reach none of it.

**The difference between the two is where the argument block comes from.**
`+0x34` walks one out of the container at `unit+0xE0` — two pointer hops and an
8-byte stride. `+0x44` goes through the wrapper that **builds** one on its own
stack from a vector and two angles. Same transform underneath, two ways of being
told what to apply.

So the unit has two virtual entry points into one kernel, which is a coherent
design rather than a coincidence of addresses.

## Nothing in the interface reads `+0x10`

No method touches either locator **directly** except the destructor at `+0x00`,
which writes vtables at `+16` and `+128` — construction bookkeeping, not use.

That scan sees direct access only, so it was extended: a method could pass `this`
to a free function that adds the offset itself, which is exactly what
`0x822A1E80` does with `+0x80`. Following the calls closes that gap, and the
answer does not change. **Every path the unit's own interface offers reaches the
locator at `+0x80`.**

So whatever reads `+0x10` is not a `CAce6Unit` virtual method. For the first time
in this thread that is a conclusion from a complete population rather than from a
plausibility filter over a scan.

## Not established

- What reads the locator at `+0x10`. It is a free function, or another class's
  method, and four probes have now each removed a possibility: the address-of
  scan, the interface, the type, and the unit's own vtable.
- What writes `unit+0xE0`, which `+0x34` depends on.
- Whether `CAce6UnitPlayer` overrides `+0x34` or `+0x44`. Its vtable is also 23
  slots and was not compared slot by slot.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Compare the three unit vtables slot by slot. It is 23 words against 23 against
31, it is free, and it answers two questions at once: whether the player
specialises the transform entry points, and what the eight extra slots of
`CAce6UnitOtherPlayer` are — a class that has more slots than the one it appears
to derive from is worth a second look before anything is built on the hierarchy.
