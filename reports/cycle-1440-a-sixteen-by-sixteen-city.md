# Cycle 1440 — a sixteen-by-sixteen city

## Qualification

- **No Ghidra run and no oracle pass.** The extracted archive and the product's
  decoder.
- No product C++ changed; ctest stays **53**. **No contract entry.**

## The parts are local

The cheapest test first: are the 178 map parts already in world coordinates?

```
min_x spans  -258 ..    0
max_x spans     0 ..  256
```

Every part is centred on its own origin. They are **local**, and something else
places them.

## Three blobs beside them

`021_FHM/` holds the map parts in `014_FHM/` and, beside them:

```
001_MCA_00.bin      272 bytes   'MCA\0' version 00010000
002_MCD_00.bin  211,472 bytes   'MCD\0' version 00010000
003_MCI_00.bin    9,744 bytes   'MCI\0' version 00010000
```

Three magic-tagged formats nothing in this campaign has read. And the smallest
one gives itself away by arithmetic: **272 − 16 = 256 = 16 × 16.**

## MCA is a map

```
02 11 10 02 02 02 02 02 02 0f 02 02 02 02 02 02
02 02 11 10 02 02 02 02 02 0f 02 02 02 02 02 02
02 02 02 11 10 02 02 02 02 0f 02 02 02 02 02 02
02 02 02 02 11 10 02 02 02 0f 02 02 02 02 02 02
02 02 02 02 02 11 10 02 02 0f 02 02 02 02 02 02
02 02 02 02 02 02 11 10 02 12 02 02 02 02 02 02
02 02 02 02 02 02 02 03 04 05 02 02 02 02 02 02
02 02 02 02 02 02 06 07 08 09 02 02 02 02 02 02
00 00 00 00 00 00 0a 0b 0c 0d 0e 00 00 00 00 00
01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01
 ... rows 9..15 all 01
```

Read as a region grid it is coherent in a way random bytes are not:

- **a diagonal**, `11 10` stepping one cell right per row for six rows;
- **a vertical line**, `0f` down column 9, meeting the diagonal at `12`;
- **twelve cells, `03` through `0e`, each appearing exactly once** — a
  contiguous block of unique regions where the two lines meet;
- `02` (103 cells) above, `01` (112 cells) filling every row from 9 down, `00`
  (11 cells) between them.

A linear feature crossing another, a small block of individually-identified
cells at the junction, and two large uniform fields. **Twelve unique cells is
not something a random byte array does**, and neither is a diagonal.

I am not going to name them. Calling `01` the sea and the diagonal a river is
the move that cost three cycles at 1428–1438, and the structure is the finding.

## The other two

| | body | |
|---|---:|---|
| MCD | 211,456 | `01 9d` at +8 |
| MCI | 9,728 | `13` at +8 |

Both unread. `0x019D` is 413 and 211,456 / 413 is not integral, so the obvious
record-array reading does not hold and was not forced.

## Not established

- Everything about MCD and MCI beyond their size and magic.
- The grid's cell size. 16 cells against parts capped at 512 units suggests
  8,192 across, and the scenario's placed units span 66,456 — those do not
  obviously agree, and reconciling them is work rather than a remark.
- Which part goes in which cell. MCA gives 256 cells and there are 178 parts;
  nothing yet connects the two.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**MCD, by arithmetic before interpretation.** 211,456 bytes is the largest of
the three and the likeliest to carry per-cell or per-part records. The first
question is its record size, and the way to get it is the way MCA gave itself
up — divisors that come out whole, then a look at the bytes, then a reading.

Not the reverse. Cycle 1426 cost three arbitrations for proposing readings of
bytes it had not looked at, and MCA took one `xxd` because the size factored.
