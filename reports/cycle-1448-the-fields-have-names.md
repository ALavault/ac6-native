# Cycle 1448 — the fields have names

## Qualification

- **No Ghidra run and no oracle pass.** The flat image, read with
  `tools/ppc_read.py`, and `exports/` used only to locate functions.
- No product C++ changed; ctest stays **54**. **No contract entry** — the port
  is the next cycle's work; this one is the derivation it will cite.

## Bounding the population, because the first scan failed

A scan for stores to `+0x0C`, `+0x10`, `+0x34`, `+0x38`, `+0x3C` over the whole
image returns **4,504 sites and 69 candidate windows** — the "577 candidates and
zero information" shape the plan names. Restricted to `CMapManager`'s own code,
`0x820F9000..0x82104000`, it returns **51**, of which two windows write all five
with live registers. One of them is the constructor zeroing them.

The other is **`0x820FBC28`**, and it is the loader.

## Every field, named by retail

`0x820FBC28` formats a name, calls `0x82101A18(this, name, &size_out)`, and
stores the result. The names are at `0x8205BDA8` onward:

| field | size at | file | in the archive |
|---|---|---|---|
| `+0x0C` | `+0x40` | **`.mha`** | `004`, 16 x 16 patch ids |
| `+0x10` | `+0x44` | **`.mhd`** | `005`, 74 patches |
| `+0x14` | `+0x48` | `.mia` | |
| `+0x18` | `+0x4C` | `.mii` | |
| `+0x1C` | `+0x50` | `.mid` | |
| `+0x20` | `+0x54` | `.mta` | |
| `+0x24` | `+0x58` | `.mti` | |
| `+0x28` | `+0x5C` | **`.pdl`** | `011`, the placement list |
| `+0x2C` | `+0x60` | `.edl` | |
| `+0x34` | — | **`.mca`** | |
| `+0x38` | — | **`.mci`** | |
| `+0x3C` | — | **`.mcd`** | |

So the convention is `m` + domain + `a`/`i`/`d` — **area, index, data**. The
heightfield is `.mha`/`.mhd`: *map height area* and *map height data*, and it has
no `.mhi`, which is exactly why cycle 1446's decode goes from the 16 x 16 grid
straight to the patch with no middle level, while `.mc*` has all three.

`/map/%s` and **`parts/%d`** are in the same table: the parts are resolved by
integer id, as cycle 1246 established for every other container.

## Retail's own arithmetic, confirming cycle 1445 from a second function

```
0x820FBEFC  lis   r11,-0x7D1        \  0xF82F04E9, the reciprocal
0x820FBF08  ori   r11,r11,0x4E9     /
0x820FBF00  lwz   r10,0x44(r31)        the size of .mhd
0x820FBF14  mulhwu r10,r10,r11
0x820FBF24  rlwinm r10,r10,18,14,31    >> 14   -> / 16900
0x820FBF0C  lwz   r9,0x4C(r31)         the size of .mii
0x820FBF28  rlwinm r11,r11,20,12,31    >> 12   -> / 4225
0x820FBF2C  rlwinm r9,r8,23,9,31       >> 9    -> / 512   (.mti)
```

**16,900 and 4,225 = 65 x 65.** Cycle 1445 derived the 65 x 65 patch from the
row step `0x104` in `0x82102568`; here is retail, in an unrelated function,
dividing `.mhd` by 16,900 and `.mii` by 4,225 — one float per sample and one
byte per sample **on the same lattice**. Two independent confirmations of the
same geometry.

It also closes cycle 1446's loose end: `005` is `74 * 16900 + 4` and retail
integer-divides, so **the four trailing bytes are slack retail never reads.**

And a self-correction: cycle 1445 called `007` "one 65 x 65 float patch" from
its size alone. Retail's divisor for `.mii` is 4,225, not 16,900, so a `.mii`
record is one **byte** per sample; `007` is four such records, not one float
patch. The size was consistent with both and I named the wrong one.

`.pdl` and `.edl` each get `if (size == 4) { free; null }` at `0x820FBF30` and
`0x820FBF5C` — a four-byte file is how "empty" is spelled.

## And cycle 1447's placement, from the code this time

`0x82102148` is the consumer of `+0x28`:

```
0x8210217C  lwz    r8,0x28(r31)     .pdl
0x82102198  lwz    r11,0x5C(r31)    its size; bail if <= 4
0x821021A4  srawi  r8,r9,4          coarse x, bounds-checked 0..15
0x821021D0  add    r6,r11,r8        index = coarse_z * 16 + coarse_x
0x82102234  rlwinm r9,r6,4,0,27     index * 16
0x82102240  add    r9,r9,r8         -> .pdl + index * 16
```

**A 16-byte header record per coarse cell**, which is what cycle 1447 read out of
the bytes. And the world transform:

```
0x8210220C  rlwinm r9,r8,13,0,18    coarse_x * 8192
0x82102214  ori    r11,r11,0xF000   61440
0x82102220  subf   r9,r11,r9        coarse_x * 8192 - 61440
```

Cycle 1447 wrote `world = cell * 8192 - 65536 + 4096` and called the local origin
the cell's centre. **`65536 - 4096` is `61440`**, and retail folds it into one
constant. The reading was right and is now derived rather than inferred.

## Something new, and unset by this loader

`0x8210218C` reads **`+0x30`** — a field `0x820FBC28` never writes — as an array
of u32 offsets indexed by coarse cell, each reaching a table of **256 eight-byte
records**, one per sub-cell (`0x821021F4` shifts the sub-index by 3). Both it and
the `.pdl` header are consulted before anything is drawn.

## Not established

- What `+0x30` is or who fills it.
- `.mia`/`.mid`/`.mta`/`.mti`/`.edl` beyond their names and sizes.
- The file-to-name assignment for anything but `.mha`, `.mhd`, `.pdl` and the
  `.mc*` triple. The container's entry order matches the loader's call order and
  the three divisors land on integers under that alignment, which is why those
  four are claimed and the rest are not.

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

**Port the placement and contract it.** The blocker cycle 1447 named is gone:
`0x82102148` and `0x820FBC28` are addresses a derivation can cite, the header
layout is confirmed from the code, and the two controls — 99.2% on land against
53.0%, 98.5% on flat ground against 50.3% — are already written and reproducible.
That is `static` + `native-test` + `derivation` on the day it is written.
