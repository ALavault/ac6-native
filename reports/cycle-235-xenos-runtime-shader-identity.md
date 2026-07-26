# AC6 cycle 235 — Xenos runtime shader identity bridge

Date: 2026-07-18

## Question

Can a shader selected by the runtime command processor be mapped back to one
exact retail Xenos container and its retained source identity without guessing
from MATE flags or filenames?

## Qualified inputs

- AC6 PAL `DATA.TBL`, `DATA00.PAC` and `DATA01.PAC` associated with the
  qualified XEX SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- pinned XenosRecomp container contract under `.tools/xenosrecomp-source`;
- external BSD-3-Clause ReXGlue/AC6_recomp reference pinned at
  `c5b089fb6988ac504ba394db611543bda2fb2c96`;
- system `libxxhash` 0.8.3, used only by the diagnostic catalogue;
- no Xenia, GUI, VNC or human input.

The external XEX identity is unpublished. External names and runtime behavior
therefore remain `cross-match`; exact ucode bytes provide the comparison key.

## Bounded ucode contract

The Xenos container stores a big-endian `Shader` record at `shader_offset`.
Its first two words are `physicalOffset` and `size`. XenosRecomp starts code at
`container.virtualSize + shader.physicalOffset` and advances instruction byte
addresses against `shader.size`. The retail entry-163 first container confirms
the unit: `size=0xb4` bytes, and that span ends exactly at the end of its
physical region.

`read_xenos_shader_ucode()` now rejects:

- truncated shader metadata;
- zero or non-word-aligned sizes;
- physical offsets outside `physical_size`;
- spans that cross the physical-region boundary.

The returned borrowed span contains the raw guest bytes, before any host word
swap. The optional catalogue hashes this exact span with unseeded XXH3-64,
matching ReXGlue/Xenia's `LoadShader` contract.

## Complete retail result

The full 926-entry scan reports:

```text
shader_containers=3808
bounded_ucode_spans=3808
runtime_hash_support=xxh3-64
runtime_hashed_shaders=3808
malformed_ucode=0
```

Entry 163 separately reports 3,806/3,806 retained identities and bounded ucode
spans, with zero malformed identities, constant tables or ucode spans. Its
1,278 `ACE_vSpecularParam` users now expose exact `.updb` basename, compiler,
stage, constant binding, ucode size and runtime hash rather than only a raw
string occurrence.

## Independent runtime-hash matches

Four hashes used by the pinned external renderer diagnostics were queried
against all 3,808 qualified retail shaders:

```text
0x7d22894002d16018 -> entry=163 member=47 offset=0x74e140
                      pixel ReductionBufferHalfRes.updb, ucode_size=0xc0
0x17e5e4ac3e713245 -> entry=163 member=47 offset=0x74e640
                      pixel ReductionBufferSynthesisPS.updb, ucode_size=0x438
0xc049a8c9e556f129 -> no retail DATA match
0x2e372ea28cc404b7 -> no retail DATA match
```

The second exact match is the compositor hash explicitly used by the external
runtime as a world-render signal. This is a content-qualified bridge from a
runtime draw to one static retail source identity. It does not prove that the
external project uses the same complete XEX or that its host fixes are retail
semantics. The two absent trail hashes remain an explicit corpus/build/runtime
boundary.

## MATE boundary clarified

The XEX name-hash routine at `0x82340088` uses MD5, not SHA-1. MD5 of
`ACE_vSpecularParam` begins `c8 91 5c 81`; the little-endian first word
`0x815c91c8` equals all observed `NU_HASH_ACE_vSpecularParam` values in the
qualified aircraft MATE. This confirms the named parameter record, but it
also rules out treating that value as the shader-permutation selector.

The exact MATE material-to-permutation relation is therefore still open. The
next non-human route is to record active runtime shader hashes at draw
submission and correlate them with the now-complete static catalogue. No
renderer equation is changed by this cycle.

## Commands

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .build/ace-combat-6 -j16 --target \
  ac6-xenos-shader-tests ac6-shader-parameter-catalog
ctest --test-dir .build/ace-combat-6 \
  -R '^ac6-xenos-shader-tests$' --output-on-failure

.build/ace-combat-6/ac6-shader-parameter-catalog \
  workspaces/ace-combat-6/game-files/DATA.TBL \
  workspaces/ace-combat-6/game-files/DATA00.PAC \
  workspaces/ace-combat-6/game-files/DATA01.PAC \
  ACE_vSpecularParam all 0x17e5e4ac3e713245
```

## Validation result

- GCC corpus: 43/43 PASS;
- Clang corpus including four XenonRecomp probes: 47/47 PASS;
- installed `bin/ac6-shader-parameter-catalog` reproduces the compositor
  lookup with 3,806/3,806 bounded entry-163 ucode spans;
- the tool fails explicitly if a runtime-hash query is requested without the
  optional `libxxhash` support;
- root install creates no `bin/bin` nesting;
- `git diff --check`: PASS.
