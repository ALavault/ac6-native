# AC6 cycle 234 — Xenos register and sampled specular equation

Date: 2026-07-18

## Question

Which Xenos constant receives `ACE_vSpecularParam`, and what operation does a
retail shader perform with the serialized vector `[3, 0.4, 0, 20]`?

## Qualified input

- target: AC6 PAL `default.xex` and its qualified retail `DATA.TBL`/PAC set;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- deterministic decoder: the existing 926/926 `DATA.TBL` payload path;
- constant-table layout: the big-endian `ConstantTableContainer`,
  `ConstantTable` and `ConstantInfo` structures consumed by the pinned local
  XenosRecomp source;
- no Xenia, GUI, VNC or human input.

## Complete constant-table scan

The native scanner validates XenosRecomp container headers, bounds every
container and then decodes the constant metadata instead of accepting a raw
string occurrence. The complete retail scan reports:

```text
scanned_entries=926
shader_containers=3808
malformed_constant_tables=0
parameter=ACE_vSpecularParam
matches=1278
binding_stage=pixel register_set=2 register_index=131 register_count=1 shader_constants=1278
```

All 1,278 matches are in decoded entry 163. They are pixel-shader `float4`
constants at `c131`, with a count of one. Register set 2 is `Float4` in the
XenosRecomp contract. This closes the register and stage boundary.

## Deterministic shader sample

The first matching container in entry 163 is at relative offset `0x6240`,
has size `0x3ec`, and has SHA-256
`2b21a7bb8f04c535a66d6619cc7975ab2b8eb36fad7863e28acda6082a8933e1`.
Retail bytes and generated HLSL stayed under `/tmp`; neither was added to the
repository. The generated HLSL SHA-256 is
`d1daf1f84def49477c3b7158d5e4d43d1a9be32f233f240ff07c4699e41aad23`.

The XenosRecomp translation names the binding:

```text
float4 ACE_vSpecularParam : packoffset(c131);
```

In this permutation:

- `.w` multiplies `log2(max(dot(...), 0))` before `exp2`, which is the
  translated `pow(max(dot(...), 0), .w)` exponent path;
- `.x` multiplies the three lanes previously formed from
  `ACE_vSpecularCol` and sampled values;
- `.y` is read in two separate additive/subtractive bias expressions;
- `.z` is not read.

For the first MATE vector, this means the sample shader observes an exponent
of 20 and a colour-lane multiplier of 3. The exact high-level semantic name of
the two `.y` bias uses remains unknown; the report preserves the translated
operations rather than inventing one.

## Confidence and remaining boundary

- **confirmed**: all retail reflection matches bind the name to pixel `c131` as
  one `float4`;
- **confirmed**: the selected retail permutation consumes `.w`, `.x` and `.y`
  as described above and does not read `.z`;
- **cross-match**: the previously recovered MATE vector and XEX named setter
  feed this reflection name;
- **unknown**: which of the 1,278 permutations is selected for each of the 48
  MATE records in the current native scene;
- **unknown**: whether every permutation uses the components identically.

No global host-side specular equation was added. The next bounded task is to
recover the material/shader selection key that maps a current MATE object to a
specific entry-163 permutation, then apply only the qualified equation.

## Validation commands

```bash
cmake --build .build/ace-combat-6 -j16 --target \
  ac6-xenos-shader-tests ac6-shader-parameter-catalog
ctest --test-dir .build/ace-combat-6 --output-on-failure \
  -R '^ac6-xenos-shader-tests$'

.build/ace-combat-6/ac6-shader-parameter-catalog \
  workspaces/ace-combat-6/game-files/DATA.TBL \
  workspaces/ace-combat-6/game-files/DATA00.PAC \
  workspaces/ace-combat-6/game-files/DATA01.PAC \
  ACE_vSpecularParam
```

Final validation:

- GCC native corpus: 43/43 PASS;
- Clang corpus including XenonRecomp probes: 47/47 PASS;
- installed `bin/ac6-shader-parameter-catalog` reproduces the entry-163
  result;
- root install contains no `bin/bin` nesting.
