# Cycle 1475 — the caller was wrong

## Qualification

- **No Ghidra run and no oracle pass.** The archive and the product.
- Product C++ unchanged. ctest stays **59**.
- **No contract entry.** `ntxr_texture` is already contracted and this cycle
  changed none of it — which is the finding.

## 177 of 177, and the decoder was right

Cycle 1474 ended with every map texture refusing `PayloadSizeMismatch` and
proposed the obvious next step: find the layout rule the decoder is missing.

**There isn't one.** Reading the descriptors from the untrimmed files:

```
64x64  format 20  mips 7  cube 0
file 53248  start 4096  payload 49152  expected 16384  base 16384  chain 32768
-> mip ok
```

All 170, every one `mip ok`. The decoder's three size checks pass. Measured
end to end:

> **untrimmed: 177 of 177 decode. trimmed: 0 of 177.**

The refusal was produced entirely by **the caller**, which trimmed the span to
`0x10 + data_offset + single_level_surface_bytes` before handing it over. That
rule is cycle 1435's and it is correct for a **single-level** wrapper whose
array 1 is padded. Every map wrapper declares **seven** levels, so trimming to
the base surface cut the chain off — and the decoder's own `base + chain ==
payload` check then refused, correctly, a file the caller had truncated.

A contracted decoder refused 177 files for a good reason and I spent a cycle
proposing to change it.

## The fix, and where else it lives

`if (mip_count <= 1)` around the trim. Two callers had it —
`tools/mission01_scene_render.cpp` and `tools/ndxr_model_textured.cpp` — and both
are fixed. It never showed in the older one because the model package's wrappers
are single-level, which is exactly why the rule looked unconditional when it was
written.

## What it looks like now

```
3853 instances drawn, 3853 with a texture; 153 textures decoded
```

`mission01-scene-textured-bridge.png`: the bay bridge in red steel with its
girders and cables, a concrete roadway with markings, and the city across the
water in individual colours — under retail's own sun, fog and 16,000-unit draw
distance from cycle 1474's mapset XML.

## Not established

- The dark grey faces. Some geometry draws untextured; whether those descriptors
  carry no texcoords, use the material's *second* texture, or need a slot this
  tool does not read is unexamined. `ntxr_texture.h` has been recording "the
  second texture of each material" as an open defect since well before this
  cycle.
- Anything about the terrain's own texture. The ground is still flat colour.

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

**The untextured faces.** 3,853 instances draw with a texture and some of their
triangles still come out grey, which means the per-descriptor material lookup —
first slot, first reference — is too coarse. The container exposes four slots
and a texture count per material; reading them is the difference between a city
that is textured and a city that is textured everywhere.
