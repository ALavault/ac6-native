# Cycle 1454 — `parts/%d`

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`, the
  archive, and the ported decoders.
- Product C++ **changed**: two assertions added to the placement test; the
  derivation header settled. ctest stays **56**.
- Contract: `retail_map_placement`'s statement, addresses and native-test claim
  updated. Still **34 behaviours**.

## The loader, found by the bounded scan

`[this+0x74]` is the bound vtable slot `+0x5C` checks. Stores to `+0x74` inside
`CMapManager`'s own code: **8**, against 4,504 for the five-field scan cycle 1448
had to bound. Two are in `0x820FBC28`, the map loader.

`0x820FC340`..`0x820FC42C` is a **256-iteration loop**:

```
0x820FC334  r19 = 0x8205BE24 = "parts/%d"
0x820FC34C  sprintf(buf, "parts/%d", i)
0x820FC36C  load(buf under the map path)  -> this[0x40B0 + i*4]
0x820FC3A4  r20 = 0x8205BFD0 = "%s.nud"
0x820FC3C8  load("parts/%d.nud")          -> this[0x6D8C + i*4]
0x820FC404  ++this[0x74] when that load succeeded
0x820FC428  while i < 0x100
```

`this+0x6D8C` is exactly the table `0x82100600` indexes. **So the nine-bit field
is the `parts/%d` number.**

## Three facts, and they agree

1. the loader keys both tables by that index;
2. the container holds **170 `NDXR` entries**, ordinals 0..169, then 86
   `unknown.bin` — and the nine-bit field runs **8..169**, entirely inside the
   models;
3. `tag & 0xFFFF` reaches **170, 171, 172**, which are not models.

The test states it in falsifiable form: every one of the 160 selectors names an
existing NDXR entry, and three of the 173 low-sixteen values name none.

## Which corrects cycle 1453, and how it went wrong

Cycle 1453 drew both readings and wrote that the nine-bit choice "tiles identical
silos inland and marches identical warehouses in a regular row out over the bay".
It leaned toward `tag & 0xFFFF` on that basis, correctly refusing to promote a
picture to a derivation — but it let the picture set the direction anyway.

The picture was misread. Drawn from low altitude, those "warehouses marching out
over the bay" are **suspension-bridge pylons carrying cables across it** —
`mission01-city-correct-models.png`. And they stand exactly on the arc of
instances the cycle-1447 placement map showed crossing the bay mouth, which that
cycle noted and did not name.

Three cycles in a row now — 1451's angle, 1452's water proxy, 1453's silos — the
eye or a fitted statistic pointed one way and reading the code pointed the other.
The code was right each time.

## What the low sixteen bits are is still unread

The pair `(tag & 0xFFFF, selector)` remains unique across all 4,226 accepted
instances — 27,680 possible pairs, zero collisions where chance predicts about
323 — so the low half carries something. `0x821023B4` extracts it and this cycle
did not follow it.

## Not established

- What `tag & 0xFFFF` is for.
- `this[0x40B0 + i*4]`, the first of the two tables, and what a `.nud` is against
  the `.ndxr` the container actually holds.
- `this+0x78`, the second counter.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 34 behaviours
ctest                                 100% passed, 0 failed out of 56
contract_addresses                    pass cited=321 supported=321
contract_derivations                  pass behaviours=52 gaps=0 multiple=0
tools/tests                           Ran 79 tests, OK
```

`tools/mission01_city_render.cpp` now defaults to the nine-bit selector. The
captures from cycles 1449–1453 drew the wrong buildings and are left in place
with that recorded, because the placement, the ground and the geometry decode
they also demonstrate are each correct and contracted.

## Next

**Follow `tag & 0xFFFF` from `0x821023B4`.** It is the last unread field of a
record whose every other bit is now derived, it is half of a key that is unique
across 4,226 instances, and the instruction that extracts it is already located —
which is the cheapest open question on the board.
