# Cycle 1442 — one bit per cell

## Qualification

- **No Ghidra run and no oracle pass.** The image and the recompiled corpus.
- No product C++ changed; ctest stays **53**. **No contract entry.**

## Why two magic searches found nothing

Searching the corpus for `MCA\0`/`MCD\0`/`MCI\0` as materialised 32-bit
constants: **zero sites**, and zero compares against `0x4D43`. Cycle 1422's
search for `FHM ` returned zero the same way, and I read that as "retail does not
validate its own archive".

It validates them. **As strings, byte by byte.** `0x82101FC8` onward is three
inline `strcmp`s against `"MCA"`, `"MCI"` and `"MCD"` at `0x8205BFD8`,
`0x8205BFDC` and `0x8205BFE0` — a `lbz`/`subf`/`cmpwi` loop, never a 32-bit
constant.

So a magic never appears as an immediate, and searching for one is searching for
an instruction pattern the compiler had no reason to emit. Two cycles concluded
"retail does not check" from a search that could not have found a check.

The string table also carries **`.mapparts.distanceL`, `.mapparts.distanceM`,
`.mapparts.distanceS`**, `.mapparts.mipBias` and `.mapparts.meshMipBias` — so
the `_l_`, `_m_` and `_s_` in the 178 part names are **draw-distance classes**,
which the census had counted (45 / 43 / 71) without knowing what they were.

## What the blobs are for

`0x82101EE8` takes a struct and a position and ends like this:

```
0x82102124  srawi  r11,r11,3      the bit index, divided by eight
0x82102130  lbzx   r11,r11,r10    the byte out of the blob
0x82102134  srw    r11,r11,r9     shifted by 7 - (index & 7)
0x82102138  clrlwi r3,r11,31      and masked to one bit
```

**It returns a single bit for a world position.** The three pointers it
validates come from `[struct+52]`, `[struct+56]` and `[struct+60]` — MCA, MCI
and MCD in that order.

Cycle 1441 established MCD *is* a bitmap from its byte histogram. This is what
reads it.

## The world-to-cell transform

```
0x82101EF4  lfs   f11,0(r4)        x
0x82101EF8  lfs   f10,8(r4)        z   -- +4, the vertical, is skipped
0x82101F14  fadds f13,f11,f0       + 65536.0        (0x82069BB8)
0x82101F24  fmuls f13,f13,f0       * 0.001953125    (0x82069BB4)
0x82101F2C  fctiwz f12,f13         -> an integer cell
```

`0.001953125` is exactly **1/512**. So

> **cell = (world + 65536) / 512**, and the world origin sits at cell 128.

**512 is exactly the cap on every one of the 178 map parts.** Cycle 1439
measured that nothing exceeds ~512 units in x or z and called it "a tiled city";
this is the tile size, read from the code that uses it, and the two agree without
being fitted to each other.

The `+65536` puts the addressable world at ±65,536 units, and the scenario's
placed units span x from −50,168 to 16,288 — inside it.

## Not established

- Which blob the bit comes from. The function validates three and indexes one;
  which pointer reaches `r10` at `0x82102130` was not traced.
- How 211,456 bytes of MCD divides over a 512-unit grid. 256 × 256 cells is
  8,192 bytes and 211,456 is not a multiple of it, so the simple reading does
  not hold and is not forced.
- What the bit *means*. A point query returning one bit per cell is a mask; what
  it masks is still open.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 31 behaviours
ctest                                 100% passed, 0 failed out of 53
tools/tests                           Ran 79 tests, OK
```

## Next

**Trace `r10` back to one of the three pointers**, which names the blob the bit
comes from and therefore what the other two are for. It is a read inside one
152-instruction function, and it is the difference between "there is a bit
query" and "MCD is the *x* mask".

The lesson to carry: **a magic is a string, not a number.** The search that
works is on the string table, and it found the reader, the three names and five
tunables in one pass.
