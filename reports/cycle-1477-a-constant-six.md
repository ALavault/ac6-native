# Cycle 1477 — a constant six

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`.
- No product C++ changed; ctest stays **59**. **No contract entry.**

## The question, and it is answered against me

Cycle 1476 ended by naming the last unexamined assumption: that every NDXR
descriptor is a triangle **strip**. `decode_ndxr_descriptor` assumes it and the
header asserts it in a comment with no citation.

`0x82364518`'s draw, at both of its call sites:

```
0x823648B4  li     r4,0x6            <- a CONSTANT
0x823648BC  lwz    r10,0x0(r11)      the descriptor's index_offset
0x823648C0  lhz    r7,0x20(r11)      its index_count
0x823648C4  rlwinm r6,r10,31,1,31    StartIndex = index_offset >> 1
0x823648C8  bl     0x821DF2C0
```

**The primitive is a constant.** It is not read from the descriptor, so there is
no per-descriptor primitive type and cycle 1476's "a list read as a strip"
cannot be what produces the large faces.

## What the constant is, and what is not derived here

Inside `0x821DF2C0` — 278 instructions building a command packet — the argument
lands in `r16` and is used three times:

```
0x821DF510  rlwinm r20,r16,0,26,31    r16 & 0x3F   -- a SIX-BIT field
0x821DF554  rlwinm r11,r16,3,0,28     r16 * 8      -- an 8-byte table index
0x821DF6F8  rlwinm r11,r16,3,0,28     the same, on the other path
```

A six-bit field in a draw packet is the shape of a primitive-type selector, and
that much is read. **That `6` means a triangle strip is the Xenos
`DI_PRIMITIVE_TYPE` convention and is NOT derived here** — the same line cycle
1432 drew when it declined to derive that a resource type word of 2 is
`D3DRTYPE_INDEXBUFFER`.

So the header's uncited "triangle strips" now has support: a constant, six bits
wide, in the draw's own packet, identical at both sites. It has not become a
derivation.

## Which leaves the grey faces unexplained

Three hypotheses have now been offered and all three refused by their own
controls:

| cycle | hypothesis | refuted by |
|---|---|---|
| 1475 | the decoder's size rule is wrong | untrimmed 177/177 decode |
| 1476 | one texture per part is too coarse | identical image; 170 models, 170 ids |
| 1477 | some descriptors are lists, not strips | the primitive is a constant |

The measured facts stand: 57,479 triangles, **1** degenerate, 0.445% with an
edge over 400 units against parts capped near 512. Those triangles are in the
index data and a strip walk is what retail does with it.

I have no fourth hypothesis, and saying so is the result.

## Not established

- What the large faces are. Possibly correct geometry seen without the alpha or
  the second texture that would hide it; possibly a vertex-format error that
  places a minority of vertices wrongly. Neither has been tested.
- The Xenos primitive enumeration, deliberately.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 59
instrument_discipline_index             pass shapes=36 unindexed=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

## Next

**Look at one offending part on its own.** Three cycles have theorised about
57,479 triangles in aggregate; none has isolated the bridge model, drawn it
alone, and looked at which descriptor contributes the spanning faces. That is a
smaller question than any of the three that failed, and it is the one that
would have answered them.
