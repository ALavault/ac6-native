# Cycle 1245 — the Type codes decode, and the product's reads are right

Cycle 1233 corrected the UV offset and left the `Type` codes unread, warning that
a packed format decoding to plausible floats would leave the values wrong anyway.
They are read now, and the product is right.

## How the decoder was found — a uniqueness argument

`0x821DE790` (SetVertexDeclaration) does `stw r4,0x2e24(r3)` and nothing else.
A byte-level scan of displacement `0x2E24` over **all** of `.text` returns 103
hits: **102 stores and exactly one load**, `821ed218`. That single read reaches
`0x821EC598`, which emits 12 bytes per element and is building Xenos `vfetch`
instructions.

One load among 103 accesses is the kind of bottleneck that makes a search
finite. It also gave a third independent confirmation of the 12-byte element and
of `Usage` at `+9` / `UsageIndex` at `+10`, from the matching code at
`821ec638` / `821ec64c`.

## The layout, and the size table read here

| `Type` bits | meaning |
|---|---|
| 0..5 | Xenos vertex-fetch **format** |
| 6..7 | gates a swizzle rewrite (`0x40` only on 16-bit-component formats) |
| 8..9 | sign / normalized |
| 10..21 | four 3-bit swizzle slots |
| 22..31 | zero in all eleven codes in the image |

The size table at **`0x82068278`**, dumped and decoded here rather than taken on
report — values in **dwords**:

```
6→1  7→1  16→1  17→1  25→1  26→2  31→1  32→2
33→1 34→2 35→4  36→1  37→2  38→4  57→3     all others 0
```

| code | fmt | size | field |
|---|---|---|---|
| `0x002A23B9` | 57 | **12 B** | POSITION |
| `0x002C23A5` | 37 | **8 B** | TEXCOORD |
| `0x001A2360` | 32 | 8 B | NORMAL |
| `0x001A2086` | 6 | 4 B | COLOR |

**All four match the offsets cycle 1233 derived from the declaration records** —
two independent readings of different bytes agreeing.

So **TEXCOORD is `32_32_FLOAT`, two `float32`, and POSITION is `32_32_32_FLOAT`,
three.** The product reads both exactly that way.

## The decisive control is a field the product never reads

Plausibility on TEXCOORD behaves exactly as cycle 1242 predicted it would: the
derived decode scores 94.5% at `|v| <= 64`, and a rival — four `int16`
normalized, **the same size** — scores **100%** and survives. The weak control
cannot separate them.

**NORMAL settles it, and the product does not consume NORMAL at all.** If it is
four `float16`, its `(x,y,z)` should be unit length:

| decode | unit within 2% | median `|v|` |
|---|---|---|
| **4 × float16 (fmt 32)** | **99.20%** | **1.0000** |
| 2 × float32 | 0.00% | 0.0035 |
| 4 × int16/32767 — *same size* | 0.68% | 0.4688 |
| 4 × uint16/65535 | 3.90% | 0.2344 |
| 4 × float16, bytes swapped | 0.00% | 0.0000 |

A geometric invariant the format must satisfy, on a field with no stake in the
answer. That is the shape cycle 1242 said was missing for POSITION and could not
construct — here the data supplied one.

The `int16` rival on TEXCOORD dies separately, on byte-position entropy: under
it, bytes 0/2/4/6 would all be high bytes of shorts and alike; measured, the
ratio is **14.1**. Under two `float32`, only bytes 0 and 4 are sign+exponent —
measured ratio **0.94**. Byte 0's value set is `{00, 3B–43, BB–C2}`: IEEE
exponents, no `7F`/`FF`, so no inf or NaN.

**And the instrument was measured before it was trusted.** The same
byte-entropy test does **not** fire on NORMAL — no 4-byte period, no clean 2-byte
one. It is a wide-dynamic-range `float32` detector, not a general float detector,
and it was used only where its positive control fired.

## Two corrections to what I briefed

- **`0x001A2360` is not only NORMAL.** In two T18 entries it carries Usage
  `0x0A` = COLOR — a vertex colour of four half-floats. Likewise `0x002A23B9`
  appears as NORMAL, TANGENT and BINORMAL, not only POSITION. **A `Type` code is a
  format, not a field.**
- **The four declarations at `0x820110F0`–`0x82011108` are padded**, not FLOAT4.
  Read alone their 16-byte spacing suggests a 16-byte position; the tightly
  packed ones put the next element at `+0x0C`, and the size table says 3 dwords.
  That is why size closure is exact on 22 of 26 declarations and merely fits on
  the other four.

## Not established, stated plainly

- **Float versus integer for a given format number.** The size table cannot
  separate 34 from 37, 33 from 36, or 35 from 38 — identical dword counts, and
  nothing in this image names them. That 37 is *float* rests on the public Xenos
  enumeration plus the corpus's IEEE exponent structure: **an argument and a
  measurement, not a reading.**
- **Which swizzle slot is X.** The four 3-bit slots are read and their count
  matches the component count in 7 of 7 codes; the assignment passes through a
  masked lookup at `0x820524D0` that was not followed. X was assigned as the only
  choice putting the constant-`1` slot on W — a convention argument.
- Bits 6..7 beyond the gate, and bits 22..31, which are zero everywhere.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
size table at 0x82068278 dumped and decoded here; four sizes match cycle 1233's offsets
```

No product code changed — **because it was already right**, which is the first
time this session that reading further confirmed the product instead of
correcting it.
