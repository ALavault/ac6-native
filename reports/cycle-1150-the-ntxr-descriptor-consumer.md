# Cycle 1150 — the NTXR descriptor consumer, and the probe has been decoding the wrong format

## The chain

`0x8233EA78` is the consumer. Everything the descriptor means is decided in the
four functions it calls.

```
8233ea84  or   r31,r5,r5          ; r31 = the descriptor
8233ea94  addi r6,r1,0x50         ; three out-slots
8233ea98  addi r5,r1,0x54
8233ea9c  addi r4,r1,0x58
8233eaa8  bl   0x8234b360         ; decode the descriptor
8233eac4  bl   0x8234b118         ; cube-map flag
8233ead0  bl   0x8234b128         ; mip count
8233eaf4  bl   0x8234aed8         ; the factory
```

**`0x8234B360` — the field extraction.**

```
8234b360  lhz    r11,0x14(r3)     ; -> out at r1+0x54
8234b368  lhz    r11,0x16(r3)     ; -> out at r1+0x50
8234b370  lbz    r11,0x13(r3)     ; the format code
8234b374  cmplwi cr6,r11,0x2f     ; >= 47 -> return 0, the whole load fails
8234b380  rlwinm r11,r11,0x3,...  ; * 8
8234b384  addi   r10,r10,0x67c0   ; table at 0x826767C0
8234b38c  lwzx   r11,r11,r10      ; -> out at r1+0x58
```

**`0x8234B128`** is `lbz r3,0x11(r3)` — the mip count.
**`0x8234B118`** is `lwz r11,0x1c(r3); rlwinm r3,r11,0x17,0x1f,0x1f` — bit 9 of
word `+0x1C`, the cube-map flag.

**`0x8234AED8` — the factory.** Cube flag set → `TextureContextCubeMapXenon`
(`0x8234EEC8`); else mip count == 1 → `TextureContextXenon` (`0x8234EBA8`); else
`TextureContextMipMapXenon` (`0x8234FB08`).

**`0x8234AA68` — the base constructor**, which fixes the field mapping:

```
8234aa7c  stw r11,0x0(r3)   ; vtable 0x82010CC0 = NU::Texture::TextureContext
8234aa98  stw r4,0x8(r3)
8234aa9c  stw r6,0xc(r3)    ; <- lhz descriptor+0x14
8234aaa0  stw r7,0x10(r3)   ; <- lhz descriptor+0x16
8234aaa4  stw r5,0x14(r3)   ; <- table[format].word0
```

**`0x8234EC38` — the load.** It passes `this+0x0C` and `this+0x10` to
`0x821FBE30` as first and second argument, and masks the low six bits of
`this+0x14`, switching on twelve values (`0x0B 0x0C 0x11 0x14 0x26 0x28 0x2C
0x31 0x32 0x35 0x39 0x3D`). Those six bits are the Xenos `TextureFormat` field;
the table word is a packed fetch-constant template.

The vtable names come from J2's class map, which is what made this readable at
all: `0x82010CC0 NU::Texture::TextureContext`, `0x820125B8 …TextureContextXenon`,
`0x820125F4 …CubeMapXenon`, `0x82012690 …MipMapXenon`.

## The descriptor, named

`r3` points at file offset `0x10`, so in file terms:

| file offset | field | qualified wrapper `0x10002215` |
|---|---|---|
| `0x21` | mip count | 1 |
| `0x23` | format code, `< 47` | 1 |
| `0x24` halfword | **width** | 512 |
| `0x26` halfword | **height** | 512 |
| `0x2C` bit 9 | cube-map flag | 0 |
| `0x30` word | data offset from `0x10` | 4080 → data at `0x1000` |

The format table at **`0x826767C0`** holds **exactly 47 entries of 8 bytes**,
which is the bound the code checks — a self-consistency check I did not have to
assume. Word 0's low six bits are the Xenos format; word 1 takes values
{1, 2, 4, 8, 16}.

## Cycle 1149 was asking with the wrong instrument

Cycle 1149 tried to earn "word 5 is width and height" by correlating payload
sizes, got 83% against a 71% shuffled null, and correctly refused to name the
field on that evidence. The refusal was right; the method was the problem. The
answer was never in the data — it is two `lhz` instructions at descriptor `+0x14`
and `+0x16`, and which one is width is settled by the order `0x8234EC38` passes
them, not by any property of the corpus.

`NTXR_STRUCTURE_REPORT.md`'s "no field is yet named as width, height, format or
mip count" can now be lifted for those four fields, by derivation.

## The correction the derivation forces

`probe_ntxr_bc.py` decodes the qualified wrapper as **BC3 / DXT5**. The
descriptor says format code 1, table entry `0x1A200153`, low six bits `0x13` —
**k_DXT2_3, which is BC2 / DXT3**.

BC2 and BC3 share their colour half byte-for-byte: the same BC1 block in bytes
8–15. They differ only in alpha — BC2 stores sixteen explicit 4-bit alphas in
bytes 0–7, BC3 stores two endpoints and 3-bit indices. So the probe's RGB was
exactly right, which is why the report saw "an intelligible aircraft diffuse
atlas", and its alpha has been wrong the whole time in the least conspicuous
channel of the picture used to validate it.

This is the "never declare parity by eye" anti-goal catching a live error, and
it took a derivation to see it. No amount of looking at that atlas would have
found it.

## The format census, derived rather than assumed

Over the 692 `NTXR` wrappers, reading byte `0x23` and the table:

| code | table low6 | format | wrappers |
|---:|---|---|---:|
| 2 | `0x14` | k_DXT4_5 (BC3/DXT5) | 656 |
| 19 | `0x06` | k_8_8_8_8 (ARGB8888) | 22 |
| 0 | `0x12` | k_DXT1 (BC1) | 12 |
| 1 | `0x13` | k_DXT2_3 (BC2/DXT3) | 2 |

So BC3 *is* the dominant format by a wide margin — the probe's choice is right
for 656 wrappers and wrong for the one it was validated on.

## What is still not established

- **The payload multiples.** `payload / (W · H · word1/4)` is 1.0 for only 96
  wrappers; the mass sits on 2, 4, 12 and 16, with a tail at 4/3 where a mip
  chain would be. Mip count and the cube flag are now readable, so this is a
  finishable calculation, but it is not finished and I am not asserting a
  surface layout.
- **`word1`'s meaning.** It correlates with format size (DXT1 → 2, DXT3/DXT5 →
  4, k_8 → 1) but the exact unit is not pinned, so the table above uses it only
  as a relative scale.
- **The twelve special-cased formats** in `0x8234EC38` are not decoded.

## Decided rather than asked

Still no C++ decoder. Four descriptor fields are now derived and that is the
blocker cycle 1149 named, but the surface layout — how mips and cube faces are
laid out in the payload — is exactly what a decoder needs next and it is the
open item above. Writing the decoder now would mean guessing that layout, which
is the same mistake one field later.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```

No product code changed. `tools/ghidra_scripts/Ac6Xrefs.java` and `Ac6Bytes.java`
are added: the first asks Ghidra's reference manager instead of matching
disassembly text, the second dumps raw memory ranges, and neither existed when
cycle 1149 concluded the NTXR literal was unreferenced.
