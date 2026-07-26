# AC6 cycle 258 — MaterialBind bundle qualification and shader-description factory

Date: 2026-07-19

## Question

Does `ac6_material_bind_xex_boundary_v1.zip` close a MATE-to-draw bind, and
what new static boundary can be qualified from its negative result?

## Qualified identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- image base: `0x82000000`;
- `DATA.TBL` SHA-256:
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`;
- archive SHA-256:
  `7acf3070750c9e7ac0aebf9fc37d1c3112adf859c946f3ac23d1346aaee5d0f6`;
- Ghidra project: canonical `ace-combat-6`, read-only and headless.

## Archive qualification

The ZIP has safe relative members, passes `unzip -t`, and all 12 entries in
its `SHA256SUMS` verify. Its standalone breadcrumb join test passes all nine
cases, including missing material, alias cardinality, hash-preimage mismatch,
state mismatch, context mismatch and absence of a backend draw.

The supplied scanner was replayed against the repository's qualified XEX and
`DATA.TBL`. It reproduces the exact identities and the zero-based entry-163
record at file offset `0xA38`, record SHA-256
`321ca1d2bf25832fd28a75be0ae2b876ba5bfe8162f3bb7c1fdfa366968f5627`.

Its conclusion is retained:

```text
NO_QUALIFIED_MATERIAL_BIND
```

The bundle does not contain a static path joining MATE identity,
technique/pass/permutation and a draw with cardinality one. Its specular
parameter candidates and direct draw callers remain negative evidence, not a
material bind.

Two environmental limitations in the bundle are now stale rather than project
blockers: the canonical Ghidra project is available locally, and the current
AC6Recomp checkout is available at base commit
`c5b089fb6988ac504ba394db611543bda2fb2c96` with the bounded draw-time changes
from cycles 250–256. No patch from the bundle is applied.

## ShaderContextXenon factory boundary

The local headless analysis closes the next edge below MATE without claiming
the missing join.

Platform factory `0x8234BDD8` selects storage by the 32-bit slot key:

- reserved unsigned values `>= 0xFFFFFFF0` use an inline slot selected by the
  low nibble with stride `0x218`;
- other values use allocator `0x8234CB00`;
- constructor `0x823500C8` creates `NU::Shader::ShaderContextXenon`;
- `0x8234AE78` registers the new object under the same key.

Public factory `0x8233F250` publishes the object through its output pointer and
takes one reference. The shader-description input is not consumed by the
platform factory: its caller retains it and later invokes vtable byte slot
`+0x10`.

General create path `0x82343F60` preserves:

```text
r5 = context slot/key
r6 = shader-description input
r7 = secondary initializer input
```

It looks up or creates the context, then calls slot `+0x10` with
`r4=original r6` and `r5=original r7`. No direct branch, Ghidra reference,
address materialization or non-`.pdata` exact pointer to `0x82343F60` was
found, so this helper is not promoted to an active MATE caller.

> **Erratum (cycle 259):** this negative caller result was caused by an
> incomplete raw-call scan and is superseded. The PAL XEX contains the direct
> active call `0x823385A8 -> 0x82343F60`. The caller iterates `NSXR`
> shader-description entries loaded from zero-based `DATA.TBL[163]`. This
> promotes the helper to an active shader-resource path, but still does not
> establish a MATE technique/pass/permutation join. See
> `cycle-259-entry163-nsxr-shader-registration.md`.

## Built-in shader-description layout

Several static initialization paths use reserved context keys `-9` through
`-15`. One exact example at `0x823412F0` uses key `-12` and a container rooted
at `0x82666940`.

The relative-pointer contract is confirmed:

```text
payload = container + 0x20                         // 0x823440A8
vertex_level_1 = payload + u32be(payload + 0x08)  // 0x823440C0
pixel_level_1  = payload + u32be(payload + 0x0C)  // 0x823440D0
vertex_input   = payload + u32be(vertex_level_1)  // 0x82344100
pixel_input    = payload + u32be(pixel_level_1)   // 0x82344100
```

Those final pointers feed the already qualified `ShaderContextXenon`
initializer and then the vertex/pixel object constructors. This is a confirmed
shader-description boundary, but the built-in containers do not expose a MATE
record identity or technique/pass/permutation selector.

## Validation

- archive compression test: pass;
- archive manifest hashes: **12/12 pass**;
- archive synthetic join cases: **9/9 pass**;
- `VerifyShaderContextFactoryContracts.java`: **24/24 assertions pass**;
- `VerifyShaderContextXenonContracts.java`: **22/22 assertions pass**;
- exact data-pointer scan: one hit for `0x82343F60`, in `.pdata` only;
- direct raw caller scan for `0x82343F60`: zero hits;
- direct raw caller scan for `0x8234DFC8`: only `0x8234403C`.

No native source, generated/config file, Xenia, GUI, VNC or human action was
used. Native AC6 CTest remains **44/44 PASS** from cycle 256 because this cycle
changes only read-only headless diagnostics and documentation.

The zero-hit result above is retained as the historical cycle-258 execution
result, not as current truth; the cycle-259 erratum supersedes it.

## Remaining boundary

`NO_QUALIFIED_MATERIAL_BIND` remains the honest result. The next autonomous
step is to find an active caller that preserves a resource/MATE receiver into
one of these shader-description inputs, or to classify the built-in contexts
as separate renderer infrastructure. A dynamic breadcrumb remains optional
future evidence, not a request for a human run.
