# AC6 cycle 260 — MATE material key to active ShaderContext

## Scope and identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- canonical Ghidra project:
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`;
- method: native DATA/FHM extraction plus Ghidra
  `analyzeHeadless -readOnly -noanalysis`.

This pass supersedes the cycle-259 statement that no MATE identity was joined
to an NSXR ShaderContext key. It also qualifies the selected-material to draw-
request edge and the request's separate shader-context bind to indexed draws.
It does not claim that the MATE key and the request shader key are equal, nor a
cardinality-one join to a backend draw.

## Archive qualification

The visible `ac6_material_bind_xex_boundary_v1.zip` is byte-identical to the
bundle already qualified in cycle 258:

- archive SHA-256:
  `7acf3070750c9e7ac0aebf9fc37d1c3112adf859c946f3ac23d1346aaee5d0f6`;
- safe relative membership;
- **12/12** manifest hashes pass;
- **9/9** standalone breadcrumb-join cases pass.

Its `NO_QUALIFIED_MATERIAL_BIND` result was correct for its constrained
environment, which explicitly lacked the canonical Ghidra project and current
AC6Recomp checkout. It remains useful negative evidence for the rejected
specular and direct-draw candidates, and its exact-cardinality and
`host_draw_emitted` gates remain useful. Its global conclusion is now
superseded for the MATE-to-ShaderContext edge by the qualified local evidence
below. No bundle patch is applied.

## Corpus-wide key equality

`analyze_mate_shader_keys.py` extracts only temporary retail payloads, walks
the recursive FHM paths already recorded in `fhm-asset-manifest.csv`, parses
the MATE material table, and compares `u32be(material+0)` with the keys
extracted independently from the 50 NSXR containers of zero-based DATA entry
163.

The exact result is:

- 122 DATA entries containing MATE payloads;
- 733 MATE payloads;
- 2,576 material records;
- 2,576/2,576 `material+0` words present in the entry-163 NSXR key set;
- zero unmatched words;
- 14 unique MATE material keys.

This is a corpus-wide exact equality, not a probabilistic scan. One key,
`0x34000802`, occurs in two serialized NSXR descriptions in container 2.
Therefore a material key identifies a ShaderContext registry key but does not
always identify one unique serialized description. Code and reports must
preserve that ambiguity.

Evidence:

- `artifacts/ac6-cycle260-entry163-nsxr-context-keys.json`;
- `artifacts/ac6-cycle260-mate-material-key-coverage.json`;
- `workspaces/ace-combat-6/scripts/analyze_mate_shader_keys.py`.

## Active XEX path

The PAL XEX independently confirms that the matching word has the same role
at runtime.

`0x8233EF88` is the lazy material-context resolver:

1. tests bit `0x4000` in `u16(material+8)`;
2. loads the registry key from `u32(material+0)`;
3. looks it up through registry `0x828CCB80` and `0x8233F2B0`;
4. stores the resolved object at `material+4`;
5. resolves the material's linked parameter records through `0x8233EDA8`;
6. sets bit `0x4000` at `material+8`;
7. returns `material+4`.

Material fixup `0x82355320` walks `u16(material+0x0A)` texture records from
`material+0x20` with stride `0x18`, then calls this resolver at `0x823553B4`.

Two active sibling rendering paths at `0x82362A38` and `0x82362C98` select a
material pointer from an owner table using the incoming `r10` selector. A null
slot falls back to the low two bits of `u16(owner+0x22)`. They reuse
`material+4` when resolved or call `0x8233EF88`, then invoke vtable byte slot
`+0x24` on the selected context.

For `NU::Shader::ShaderContextXenon`:

- vslot `+0x24` is `0x8234B870`; it traverses the two parameter lists at
  context `+0x10/+0x14` and invokes child vslot `+0x08` with the current state;
- vslot `+0x28` is the already qualified shader-publishing method
  `0x82350318`.

The active path therefore proves material selection, exact ShaderContext key
resolution and parameter application. It must not be described as direct
shader publication by that same selected context.

## Draw-request bridge and separate shader key

The first active selector builds a request at `0x82362C4C` through constructor
`0x82363F58`:

- selected material `r26` is passed as constructor `r6` and stored at
  `request+0x24`;
- the request vtable is `0x820153FC` and its byte slot `+0x14` is
  `0x82364980`;
- the constructor initializes `request+0x08` independently to zero;
- the selected material and request are causally joined by the subsequent call
  to `0x8233ED10` at `0x82362C5C`.

Request draw method `0x82364980` then performs a distinct shader bind:

1. loads a shader-context key from `request+0x08` at `0x82364B44`;
2. resolves it through registry `0x828CCB80` and `0x8233F2B0`;
3. invokes resolved context vslot `+0x28` with `request+0x28`;
4. reaches direct indexed draws `0x821DF2C0` at `0x82364CB0`, `0x82364D10`
   and `0x82364D3C`.

This proves two bounded chains within the same request lifecycle:

```text
selected MATE material -> request+0x24 -> request scheduling
request+0x08 shader key -> ShaderContextXenon +0x28 -> indexed draw
```

It does **not** prove `u32be(material+0) == request+0x08`. No post-constructor
writer for `request+0x08` has yet been qualified, so that field remains a
separate runtime key with an unknown writer. Zero is itself a valid entry-163
NSXR key (container 0, ordinal 0), so the constructor value cannot be dismissed
as an invalid or absent context. The earlier wording “the same selected context
reaches `+0x28`” is retired.

Evidence:

- `artifacts/ac6-cycle260-material-renderer-boundary.log`;
- `artifacts/ac6-cycle260-material-postbind-chain.log`;
- `artifacts/ac6-cycle260-material-shader-bind-vtables.log`;
- `artifacts/ac6-cycle260-material-shader-bind-region.log`;
- `artifacts/ac6-cycle260-material-shader-bind-region-tail-callers.log`;
- `artifacts/ac6-cycle260-request-vtable-methods.log`;
- `artifacts/ac6-cycle260-mate-shader-context-validation.log`.

## Rejected false paths

The two direct callers of helper `0x82333700` use static records rather than
MATE materials:

- `0x821126AC` uses `{0x09050073, 0}` at `0x826E81F8`;
- `0x821189A4` uses `{0x09050071, 0}` at `0x826E8200`.

Neither key belongs to the entry-163 NSXR set. These calls do not close the
MATE bind. Immediate-key calls to sibling helper `0x823336C0` are similarly
not evidence that a MATE record reaches the draw.

## Validation

- archive manifest hashes: **12/12 pass**;
- archive standalone join cases: **9/9 pass**;
- corpus scan: **2,576/2,576 exact key matches**, zero unmatched;
- `VerifyMateShaderContextBindingContracts.java`: **58/58 assertions pass**;
- Ghidra operations were headless and read-only;
- no Xenia, GUI, VNC or human action was used.

No native runtime source changed in this pass, so the last full AC6 native
result remains **44/44 PASS** from cycle 256.

## Remaining boundary

The previous global `NO_QUALIFIED_MATERIAL_BIND` label is too broad and is
retired. The current bounded status is:

```text
MATE_MATERIAL_KEY_TO_SHADER_CONTEXT_CONFIRMED
MATE_SELECTION_TO_DRAW_REQUEST_CONFIRMED
REQUEST_SHADER_CONTEXT_BIND_TO_DRAW_CONFIRMED
MATE_KEY_TO_REQUEST_SHADER_KEY_RELATION_UNKNOWN
MATERIAL_SHADER_TO_HOST_DRAW_NEEDS_EVIDENCE
```

Cycle 261 supersedes the open-writer question for this request family: both
qualified queue routes preserve the constructor's zero until the draw slot,
while every retail MATE `material+0` key is nonzero. The remaining autonomous
task is the native capture join at `0x82364B44`, followed separately by a
qualified backend-emission marker. The archive's exact cardinality-one and
`host_draw_emitted` acceptance gates apply only after those native edges are
closed.
