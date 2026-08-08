# Cycle 1191 — the product's NDXR version check is the high byte of a size

## Measured on the 292 meshes

```
byte +0x04, the product's version gate     0 in 292 of 292
byte +0x05                                 0 x270, 1 x10, 3 x6, 2 x3, 4/5/10 x1 each
version at +0x08/+0x09, retail's reading   512 (0x0200) in 292 of 292
```

## What the product actually tests

`load_verified_binary` accepts a slice when `memcmp(raw, "NDXR", 4) == 0 &&
raw[4] == 0`, and the second clause reads as a version check. It is not one.

`be32(4, declared_size)` — two lines later in the same function — reads the byte
count from offset 4. So `raw[4]` is **bits 24–31 of that size**. It is zero for
every mesh because every mesh is smaller than 16 MB, and byte `+0x05` varies from
0 to 10 exactly as bits 16–23 of a size should for files up to 703,910 bytes.

The gate passes 292 of 292 by arithmetic, not by agreement. A 17 MB NDXR would
be rejected as a bad version.

## What retail reads instead

`0x8234CA28` returns `(byte+0x08 << 8) + byte+0x09` for an NDXR. Across all 292
that is **0x0200** — one value, no exceptions. A version field behaves like that;
a size byte does not.

## Why this is worth a cycle before the delegated read lands

Cycle 1190 flagged the `+0x04` versus `+0x08/+0x09` disagreement and sent it to a
fresh pass. This half of it needed no code reading at all: the bytes settle it,
and they settle it against the product.

It is also the concrete form of cycle 1189's finding. "The NDXR header layout is
measured and unaudited" is an abstract worry until one of the measured fields
turns out to be a misreading that happens to pass. This one is benign — the check
is vacuous rather than wrong-headed, and no mesh is mis-decoded by it — but it is
exactly what an unaudited format description produces, and it was found by
looking rather than by a failure.

## Not concluded

Whether `0x0200` means version 2.0, and whether `0x8234CA28`'s reading is itself
the whole story — that is the delegated read's job. What is established here is
that the product's `raw[4] == 0` is not testing what its position in the code
suggests.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
three live contracts                                ->  audit-valid
```

No product code changed. Changing the gate before the header parse is derived
would be fixing a symptom ahead of the reading that defines it.
