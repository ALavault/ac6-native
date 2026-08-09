# Cycle 1447 — the city is placed

## Qualification

- **No Ghidra run and no oracle pass.** The extracted archive, and the two grids
  cycles 1445 and 1446 derived from the image.
- No product C++ changed; ctest stays **54**. **No contract entry** — nothing is
  ported yet, and no retail function has been read for this.
- New: `tools/mission01_map_instances.py`.

## A reading proposed and refused, first

`013_00_02__00.bin` is 197,632 bytes = **386 x 512**, the same block shape as
MCD's 413 x 512, and its nibble alphabet is tiny — 96% of all nibbles are 0 or
1. `009_00_01_02_03.bin` is a 16 x 16 grid of 24 ids and `010` is 24 x 256 u16,
which is MCA/MCI's shape exactly. So the obvious reading is that `009`/`010`/
`013` is a second three-level grid.

**It is not.** `010`'s largest value is 1575, which needs `1575 * 512 + 512` =
806,912 bytes, and `013` has 197,632. The reading is refused on arithmetic
before anything was built on it.

What `010` *is* was established instead: group 0 holds 0,1,2,…,255 and every
later group reuses earlier values — a deduplicating index over **1,576 distinct
cell records**, with the sea group a single constant, 842, in all 256 cells. No
blob in this container has a size divisible by 1,576, so **the payload those
indices address is not in this container.**

## And the thing that has been open since cycle 1440

Cycle 1440 measured that the 178 map parts are local — every one centred on its
own origin — and wrote "something else places them". `011_00_00_00_00.bin` is
that something.

```
header   256 records of 16 bytes, one per coarse cell, cz * 16 + cx
         { u32 count; u32 offset; u32 ?; u32 zero }
body     `count` instances at `offset`, 16 bytes each
         { float x; float y; float z; u32 tag }
```

**The header is a partition and that is checked, not assumed**: its 256 counts
sum to **4,318**, and `(73,184 − 4,096) / 16` is **4,318**. The first data
offset, `0x1000`, is the constant sitting in every empty header row.

Positions run to ±4096 and a coarse cell is 8,192 units, so the local origin is
the cell's **centre**:

> `world = cell * 8192 − 65536 + 4096 + local`

## Two controls, from two other functions

The placement is tested against structures derived from **different blobs by
different retail functions**, each with a null model — because "99% on land"
means nothing until a random scatter is scored.

```
                                        placed    random scatter
water bit clear (0x82101EE8)             99.2%          53.0%
ground below 1.0 (0x82102568)            98.5%          50.3%
```

4,318 instances, spanning x −18,426..16,357 and z −20,442..6,784 — a compact
city, not a scatter over the 131,072-unit map. 4,138 of them have `y` exactly
zero; the rest run −23.6 to 334.9.

Drawn over the heightfield they are a city hugging the bay, on both banks of the
river, with an outlying district upriver and an arc of instances across the bay
mouth — `reports/mission01-terrain/mission01-map-instances.png`.

The two grids were derived to answer other questions. They agree with this one
without being asked to.

## Deliberately not named

- **`tag >> 16`.** It varies over 171 values and a rotation is the obvious
  reading. Cycles 1428, 1440 and 1441 each show what the obvious reading is
  worth here; a control exists — drawing the parts and looking — and it belongs
  to the cycle that does it.
- The header's third field.
- `012`, whose records interleave with nothing established.

## Not established

- **No retail function has been read for any of this.** The layout is arithmetic
  plus two independent controls, which is why there is no contract entry: the
  gate wants a derivation citing retail addresses and there are none to cite yet.
  That is the gap, and it is the next cycle's work rather than a caveat.
- Which part id maps to which of the 178 names. There are 173 distinct ids in
  0..172 and 178 parts, and nothing yet connects the two orders.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 32 behaviours
ctest                                 100% passed, 0 failed out of 54
contract_artifacts (three live)       pass  cited=126 match_head=126
contract_addresses                    pass  cited=307 supported=307
tools/tests                           Ran 79 tests, OK
```

## Next

**Find the reader of `011`, so this can be contracted.** Every hop of the asset
chain above it is under contract and this one is not, for exactly one reason:
the layout was read from the bytes rather than from the code, so there is no
address to cite. `CMapManager`'s constructor at `0x820FA258` initialises about
500 KB of fields and `tools/ppc_read.py` now makes reading it cheap; the field
that takes a 4,096-byte header followed by 16-byte records is a search with a
known shape, which is the position cycle 1443 was in before the magic strings
turned up.
