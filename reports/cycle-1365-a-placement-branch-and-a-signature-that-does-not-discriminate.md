# Cycle 1365 — a placement branch, and a signature that does not discriminate

## Qualification

- Ghidra was used to list call sites and data references. **No oracle pass.**
- No product C++ changed, no contract changed.

## The `obj+0x10` / `obj+0x14` question is answered, and the answer is placement

Cycle 1364 ended with one question between two contracted ends: what writes the
object fields `sub_822A2BE8` passes as two of the transform's angles.

`sub_822A2BE8` is 38 instructions, `.pdata` agreeing. Its `r4` is not a live
object — it is a **descriptor**: a flag byte at `+1`, a position at `+4`, `+8`,
`+12`, and the two angles at `+16`, `+20`. The function builds a stack vector
from the position, appends `1.0`, and hands the pair to `0x822A2B50`.

Its **only** caller reaches it by a tail branch, and is eighteen instructions:

```
sub_8224C518(descriptor):
    index = [descriptor + 0]                 a BYTE
    if index == 255 or index == 254 : return          sentinels
    table  = [[0x826E4EB4] + 0x2D3B4]
    target = [table + (index + 1) * 4]
    if target == 0 : return
    tail-branch to sub_822A2BE8(target, descriptor)
```

**A descriptor names its target by a byte index into a context table.** That is a
placement operation — *put entity N at this position with these two angles* — not
a per-frame flight update.

So the branch is identified and **excluded**. `obj+0x10`/`obj+0x14` are scenario
placement data, not flight state.

## The signature approach, tried and reported as a negative

Following data has now answered six bounded questions without reaching the
integrator, so this cycle tried the opposite: find it by what an integrator
*looks like* — a vector load, a vector multiply-add, and a vector store.

**461 functions match.** Vector arithmetic is everywhere in this binary, and that
signature is not a discriminator. Recorded as measured rather than left for a
later cycle to re-derive.

A sharper one exists and is not guessed at here: the translation row is written
by `stvx128` with an index register holding **64**, which is how cycle 1340 read
the player's locator copy (`li r5,16 ; li r6,32 ; li r7,64`). Enumerating
`li rN,64` feeding a `stvx128` whose base also feeds an `lvx128` is a much
narrower property, and it is the next thing to run.

## Where A3.2 stands, honestly

Six approaches, each bounded, each answered, none of them flight:

```
the input tick        -> commands and timers, five contracted behaviours
the transform         -> contracted as retail_transform
the unit's vtable     -> two entry points, both into the transform
the children          -> one unnamed candidate, no RTTI
the arena             -> tables at fixed offsets, unread
this branch           -> placement
```

The campaign has **fourteen contracted behaviours** and ctest at 33. What it does
not have is the integrator, and six eliminations is real progress of a kind that
does not look like it.

## Not established

- What writes a live object's orientation each frame.
- What the context table at `+0x2D3B4` holds.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

`li rN,64` feeding a `stvx128` whose base register also feeds an `lvx128` in the
same function — a read-modify-write of a locator's translation row. That is the
narrow form of the signature this cycle found too broad, and it is the kind of
enumeration that has answered four times in this thread where scans have not.
