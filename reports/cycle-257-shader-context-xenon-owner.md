# AC6 cycle 257 — qualified `ShaderContextXenon` owner boundary

Date: 2026-07-19

## Question

Is the paired owner at `0x82350318` a MATE/technique owner that can directly
close the material-to-shader join?

## Qualified identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- image base: `0x82000000`;
- Ghidra project: canonical `ace-combat-6`, read-only and headless.

## Result

No. The owner is confirmed as the retail class:

```text
NU::Shader::ShaderContextXenon
RTTI name: .?AVShaderContextXenon@Shader@NU@@
vtable:    0x820126CC
```

The MSVC chain is:

```text
vtable[-1] 0x8206B2A4
  -> TypeDescriptor 0x8267858C
  -> decorated name at 0x82678594
```

`0x82350318` occupies vtable byte slot `+0x28`; `0x82350368` occupies `+0x40`.
The constructor at `0x823500C8` publishes the vtable and clears object fields
`+0x18/+0x1C/+0x20/+0x24`.

## Paired object construction

Initializer slot byte `+0x10`, physical function `0x82350118`, consumes an
as-yet unnamed shader-description input in `r4`. It resolves two source
records, constructs the two physical D3D shader objects and retains them:

```text
0x8235016C -> 0x821DE208 -> object+0x18  (vertex, structural cross-match)
0x82350194 -> 0x821DE488 -> object+0x1C  (pixel, structural cross-match)
```

It also copies two associated source blocks into `object+0x20/+0x24`.

Bind slot `+0x28` loads those exact objects, calls the setters qualified in
cycle 255, then invokes base-context function `0x8234B8E8` with its original
second argument.

## Consequence for the MATE join

The following edge is now **confirmed**:

```text
ShaderContextXenon
  -> paired guest vertex/pixel objects
  -> bounded program references
  -> device state
  -> draw snapshot
```

The preceding edge remains open:

```text
MATE material/subrecord
  -> technique/pass/permutation or shader-description input
  -> ShaderContextXenon initializer/bind
```

Calling `ShaderContextXenon` a MATE owner would collapse two distinct layers
and is rejected. The next bounded static step is receiver-aware provenance for
the vtable `+0x10` initializer and `+0x28` bind, beginning with the `r4` input
to `0x82350118`. A raw scan of every virtual `+0x28` dispatch is not sufficient
because many unrelated classes reuse the same slot number.

## Validation

`VerifyShaderContextXenonContracts.java` passes **22/22** exact RTTI, vtable,
constructor, initializer and bind assertions. The generic virtual-dispatch
scanner was made slot-configurable while preserving its prior `+0x20` default;
its `+0x28` results are candidate sites only, not class attributions.

No native source, generated/config file, Xenia, GUI, VNC or human action was
used. The native **44/44 PASS** result from cycle 256 remains the applicable
code validation.
