# NTXR structure pass

## 2026-07-15 first exact decode follow-up

The aircraft wrapper paired with MDLP element 76 corrects the earlier
single-descriptor assumption. Byte `0x07` is a texture-entry count; this
wrapper has six fixed `0x50`-byte entries. Each entry consists of a
`0x30`-byte descriptor, `0x10`-byte `eXt` framing and `0x10`-byte `GIDX`.
Descriptor word 8 is relative to the entry start and reaches the data
allocation; word 2 bounds that allocation. All six entries end within the
512,000-byte wrapper, whose first data allocation begins at `0x1000`.

The first `GIDX 0x10002215` profile is decoded natively and fail-closed:

- dimensions: 512x512 from descriptor word 5;
- one BC3 base level: exact logical size 262,144 bytes;
- storage: Xenos tiled-2D block addresses;
- endian conversion: `8-in-16` before BC3 block decode;
- visible atlas: `captures/first-linked-0x10002215-bc3-preview.png`;
- retained wrapper: `exports/first-linked-0x10002215.ntxr`, SHA-256
  `72ab39e3d4fe0e5085fa94fad6015b55ff843dae5f93fb35028f37b86b446863`.

The tiled address implementation follows the official Xenia
`texture_address::Tiled2D` bit layout. The endian choice is additionally
validated by a negative control: omitting `8-in-16` produces visibly corrupted
colors, while the guarded native path yields an intelligible aircraft diffuse
atlas. Other descriptor profiles and mip layouts remain unsupported until
independently validated.

The native AC6 library now parses the bounded wrapper structure shared by the
retail `NTXR` texture population. The parser keeps the Xbox 360/Xenos
descriptor dwords verbatim and big-endian; it does not assign unproven width,
height, tiling or compression semantics to their bit fields.

Validated structure:

- `NTXR` signature, version byte and subtype byte;
- aligned descriptor region followed by an `eXt` chunk at offsets observed
  between `0x30` and `0x80`;
- nested `GIDX` record and its big-endian identifier;
- texture-data boundary derived from the checked `eXt` size;
- explicit 16-byte `header_only` references found in the corpus.

The full recursive retail manifest gate executed over all 926 DATA table
entries and 56,514 FHM rows:

```text
ntxr=8006 ntxr_parsed=7993 ntxr_header_only=13
```

All wrappers passed. The emitted CSV remains byte-identical to the previous
manifest, SHA-256
`e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa`,
because validation adds no proprietary bytes or speculative fields to the
inventory.

The native manifest tool now also emits aggregate descriptor distributions.
The full-corpus execution produced:

```text
ntxr_versions=1:87;2:7919
ntxr_extension_offsets=0x40:6159;0x50:197;0x60:1059;0x70:466;0x80:112
ntxr_descriptor_word_counts=12:6159;16:197;20:1059;24:466;28:112
```

There are 44 observed subtype byte values; subtype `1` accounts for 7,578
wrappers and subtype `2` for 93. The remaining 42 values form a long tail.
These are empirical populations only: no subtype or descriptor word has yet
been assigned a Xenos semantic. The complete aggregate output is retained in
`reports/ntxr-structure-summary.txt`.

## Texture payload correlation

The same full-corpus gate now measures texture bytes after the checked `eXt`
boundary, grouped only by descriptor length. Each tuple below is
`word_count:count,min,max,total,zero_count`:

```text
12:6159,4096,34869152,1335831808,0
16:197,49152,5570560,56228800,0
20:1059,13840,28512128,107963664,0
24:466,43728,18173808,249600000,0
28:112,2871136,22417248,2265364992,0
```

This proves that all 7,993 complete wrappers carry data after the extension
and establishes bounded populations for later Xenos descriptor hypotheses.
It does not prove that descriptor length alone determines texture dimensions,
format, mip count, tiling, or array depth.

## Subtype cross-check

The corpus gate now also crosses the subtype byte with descriptor length and
payload-size bounds. The complete distributions are stored in
`reports/ntxr-structure-summary.txt`. The dominant complete subtype 1 has
7,565 wrappers and occurs with every observed descriptor length:

```text
1+12:5804;1+16:197;1+20:1016;1+24:436;1+28:112
```

Therefore subtype is not a valid substitute for descriptor revision or word
count. Several long-tail subtypes also span both 12- and 24-word descriptors.
This negative invariant is enforced by retaining the two-dimensional matrix;
the decoder must use proven word fields rather than a subtype-only shortcut.

## Descriptor-position invariants

All complete descriptor words are now profiled by position using only count,
zero count, distinct-value cardinality, minimum, maximum, bitwise AND and
bitwise OR. This closes several useful negative/constant facts across all
7,993 wrappers:

- words 1, 6, 9, 10 and 11 are always zero;
- word 7 has only two values and is zero in 7,815 wrappers;
- word 8 has eight values and is nonzero in 7,919 wrappers;
- the 28-word tail has only two values at word 24, while words 25–27 are
  always zero;
- words 0, 2 and 5 have respectively 99, 56 and 287 distinct values and must
  not be collapsed into constants.

The exact aggregate tuples remain in `reports/ntxr-structure-summary.txt`.
These invariants identify candidate reserved fields and masks, but no field is
yet named as width, height, format or mip count.
