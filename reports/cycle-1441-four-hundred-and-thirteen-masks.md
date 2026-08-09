# Cycle 1441 — four hundred and thirteen masks

## Qualification

- **No Ghidra run and no oracle pass.** The extracted archive.
- No product C++ changed; ctest stays **53**. **No contract entry.**

## Correcting cycle 1440, which got its arithmetic wrong

That report says of MCD: *"`0x019D` is 413 and 211,456 / 413 is not integral, so
the obvious record-array reading does not hold and was not forced."*

**211,456 = 413 × 512 exactly.** The reading holds and I dismissed it on a
division I did not do.

The error came from printing `body / count` with `count` read as a big-endian
**u32** at `+8` — `0x019D0000`, twenty-seven million — instead of the **u16**,
`0x019D`. The ratio came out 0.008, I read that as "not integral", and wrote a
negative finding out of a field-width mistake.

Cycle 1439 had already printed `211456/413 = 512.0` and I contradicted it one
cycle later without noticing.

MCI is the same shape: **9,728 = 19 × 512**.

## MCD is a bitmap

92% of the body is `0x00` (113,695 bytes) or `0xFF` (80,421). Of the remaining
98 distinct values the common ones are `0xC0 0xE0 0x80 0xF8 0x07 0x03 0x01 0x3F
0x7F 0xFE` — **every one a single run of set bits against a clear field**, which
is what the edge of a shape looks like at one bit per cell.

413 records × 512 bytes is **413 masks of 4,096 bits**. Drawn as 64 × 64 they
are smooth contiguous regions with curved boundaries, not noise.

## And a reading I proposed and killed in the same cycle

The contact sheet of the first 64 masks shows a boundary that appears to migrate
gradually from one to the next — exactly what a stack of height levels looks
like, "cells above height N" for rising N.

Two tests, both refusing it:

```
monotonically non-increasing popcount steps :  196 of 412   (chance is ~206)
records where mask(i+1) is a subset of mask(i):  61 of 412
```

A height stack is monotone and nested by construction. This is neither. The
apparent migration was my eye joining 64 tiles that merely resemble one another.

**What the 413 masks mask is not established.** There are 178 map parts and 413
masks, and nothing connects them.

## Not established

- MCD's meaning, above.
- MCI's 19 masks.
- MCA's cell values, deliberately unnamed since cycle 1440.
- Whether the 4,096 bits are 64 × 64 at all. That is the only square factoring
  and MCA is 16 × 16, which makes it plausible and not established.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Find retail's reader.** Three cycles have now read these blobs by arithmetic
and eye, and the arithmetic keeps being right while the readings keep being
wrong — a height stack this cycle, "not integral" the last one.

`MCA`, `MCD` and `MCI` are four-byte magics. The corpus search that found the
FHM header parser at cycle 1422 works the same way here, and a function that
reads one of them settles what the masks are in a way no amount of looking at
them will.
