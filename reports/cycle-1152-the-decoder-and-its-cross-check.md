# Cycle 1152 — the decoder, and an independent implementation agreeing on 65,536 colours

## What was built

`include/ac6/ntxr_texture.h` and `src/ntxr_texture.cpp`: an NTXR decoder
restricted to single-level block textures, refusing everything else with a
named cause. `tests/ntxr_texture_tests.cpp` runs it over the whole extracted
corpus.

```
wrappers                    692
decoded                     308     (300 BC3, 6 BC1, 2 BC2)
refused: mip chain          360
refused: not block format     22
refused: cube map              2
refused: bad header            0
refused: payload mismatch      0
```

The test asserts the **partition** — decoded plus refused equals the corpus, and
the decoded set equals the independently counted single-level block population —
rather than checking a total. 38 shapes decode, 26 non-power-of-two.

## The cross-check that makes it more than a count

Every assertion above is about counts and would survive a decoder that produced
the wrong colours. So the pixels were checked against an implementation that
shares no code with them.

`scripts/probe_ntxr_bc.py` decodes every wrapper as BC3. Cycle 1150 derived that
two of the corpus wrappers are **BC2**, and predicted the consequence: BC2 and
BC3 share their colour half byte for byte and differ only in alpha, so the probe
should have exactly right colour and wrong alpha. Decoding
`idx_0119/022_FHM/006_FHM/006_NTXR.ntxr` both ways:

```
RGB identical    65536 / 65536   100.00%
alpha identical  62250 / 65536    94.99%
```

Every colour texel agrees. 3,286 alpha texels do not. That is the prediction
measured, and it does three things at once: it confirms cycle 1150's format
finding, it cross-validates this decoder's colour path against independent code,
and it quantifies how wrong the probe has been — 5% of alpha, in the one profile
family this workspace had validated by eye.

The corpus pixel hash is now asserted in the test, so the decoded output cannot
drift without failing.

## The qualified wrapper is refused, and that is correct

`exports.pre-s0/first-linked-0x10002215.ntxr` — the wrapper
`NTXR_STRUCTURE_REPORT.md` retains and the probe was validated on — is
**refused** by this decoder, cause `PayloadSizeMismatch`. It declares 512×512,
one level, BC2, which is a 262,144-byte surface, and carries 507,904 bytes after
its data offset.

The probe decodes it happily because it never checks a size. The name
`first-linked` says why: it is cut at a container boundary, not at the end of
the surface. So the file the format work was validated against is not a clean
single-surface wrapper, and nothing had noticed because nothing was measuring.

Refusing it is the decoder behaving as designed. The surface rule is asserted
before any addressing happens, so a wrapper whose payload disagrees is refused
rather than mis-addressed.

## Two of my predictions were wrong; the corpus corrected both

I wrote the test's expected refusal split before running it: 362 mip chains and
0 cube maps. It is **360 and 2** — and those two cube maps declare a *single*
level, so a mip-count check alone would have let them through into a decoder
that cannot address six faces.

I also asserted the decoded set contained no BC2. It contains exactly **2**, and
they are the wrappers the probe mis-decodes. Had that assertion been written
loosely enough to pass, the cross-check above would never have been run.

## One real bug, caught by the partition and not by reading

`kDataOffsetOffset` was written as `kDescriptorBase + 0x30`. The field is at
file `0x30`, which is descriptor `+0x20` — the descriptor-relative offsets in
`0x8234B360` run `+0x11` to `+0x1C`, and I carried that habit one field too far.

Every valid texture failed its bounds check and the decoder returned 0 of 308,
uniformly, with cause `BadHeader`. A test that had only checked "some textures
decode" would have reported a plausible-looking failure; the partition assertion
named it in one run.

## What this does not do

- It does not address **mip chains**, and cycle 1151's measurements are why.
- It does not bind a texture to anything. `MissionTextureBinding` still resolves
  from nothing, and the MATE batch→material→texture→NTXR chain is untouched.
- The **8-in-16 swap** still rests on a visual negative control, which is why
  `decode_ntxr_single_level` takes it as an argument rather than hiding it.
- No image from this decoder is offered as visual parity.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```
