# Cycle 1153 — a mip model that fits 212 of 360, and the name underneath it that was never derived

## The model

Extending cycle 1151's single-level rule to a chain: sum the tile-padded size of
each level, except that a level whose padded block dimensions are 32×32 or
smaller costs a flat **512 blocks** instead of 1,024.

```
multi-level block wrappers                       360
matched exactly by the model                     212
```

The 212 are whole shape groups, matched to the byte: 256×256 with 9 levels,
512×512 with 10, 4096×4096 with 13. The 4096×4096 case is the one worth writing
out, because it is the terrain atlas and the arithmetic is not close — it is
exact:

```
L0 1024x1024 blocks   1,048,576
L1  512x512             262,144
L2  256x256              65,536
L3  128x128              16,384
L4   64x64                4,096
L5..L12  512 each         4,096
                      ---------
                      1,400,832 blocks x 16 = 22,413,312 bytes = the payload
```

## Where it fails

```
64x64,    7 levels   144 wrappers
1024x1024, 4 levels    2 wrappers
4096x1024, 13 levels   2 wrappers
```

64×64 with 7 declared levels holds 3,072 blocks. Every level of a 64×64 texture
is 32×32 padded blocks or smaller, so the model wants 7 × 512 = 3,584. Six
levels of 512 would be 3,072 exactly, and so would three of 1,024. I can fit
either and neither is evidence.

An alternative reading — that files are padded to a 4,096-byte boundary — was
checked and rejected: the model's 3,584 blocks is 57,344 bytes, already a
multiple of 4,096, so padding cannot explain a smaller payload.

## The assumption underneath, which is mine and not the image's

I have been calling byte `+0x11` the **mip count**. What cycle 1150 actually
derived is narrower:

```
8234b128  lbz r3,0x11(r3)     ; 0x8234AED8 tests the result against 1
```

`0x8234AED8` branches on `== 1` to build `TextureContextXenon` and otherwise
`TextureContextMipMapXenon`. **That is all the image says.** The byte selects
plain against mip-mapped at the value 1; that it is a *count of levels* is my
inference from its name-shaped role, and nothing in the code multiplies by it,
loops over it, or bounds anything with it.

So a layout model built on it as a level count may be fitting the wrong
variable, and the 64×64 group — where a 6-versus-7 discrepancy is exactly what
an off-by-one in the meaning of that byte would produce — is where that would
show first.

This is the same failure shape as cycle 1149: a plausible reading that survives
because the arithmetic mostly works. The difference is that this time I noticed
before writing it into the decoder.

## Decided rather than asked

**The decoder is not extended.** 212 of 360 is not a rule, and the honest next
step is not more curve-fitting but finding what consumes byte `+0x11` as a
number — `TextureContextMipMapXenon`'s own load path, `0x8234FB08`'s sibling of
`0x8234EC38`, which this cycle did not read.

The single-level decoder is unaffected: it refuses every multi-level wrapper by
that byte being other than 1, which is exactly the derived property and does not
depend on the byte meaning a count.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed.
