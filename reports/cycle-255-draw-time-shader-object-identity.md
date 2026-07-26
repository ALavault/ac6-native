# AC6 cycle 255 — draw-time guest shader-object identity

Date: 2026-07-19

## Question

Can the existing draw snapshot preserve the exact guest shader objects active
at a draw, without relying on swap-level backend hashes or adding setter hooks
that miss inlined state changes?

## Qualified identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- image base: `0x82000000`;
- Ghidra project: canonical `ace-combat-6`, read-only and headless.

## Device fields and setters

A raw PPC D-form scanner was added because several physical D3D functions are
not persistently represented by Ghidra function bodies. It found the complete
producer/consumer set for the two committed fields:

| Role | Setter | Store | Device field | Common pre-draw load |
|---|---:|---:|---:|---:|
| compact/pixel shader object | `0x821DE2C8` | `0x821DE34C` | `+0x318C` | `0x821ED21C -> r29` |
| vertex shader object | `0x821DE5C0` | `0x821DE660` | `+0x3190` | `0x821ED214 -> r31` |

The field and ABI contracts are **confirmed**: both setters receive
`r3=device`, `r4=new object`, store the preserved `r29`, accept a null object,
and the common draw-state compiler loads the exact same fields.

The stage names are a **cross-match**, not recovered symbols. The `+0x3190`
object is interpreted through its vertex-specific metadata block at `+0x368`;
the `+0x318C` object uses the compact block starting at `+0x28`. A common owner
at `0x82350318` publishes its `+0x18` object through `0x821DE5C0` and its
`+0x1C` object through `0x821DE2C8`. The reset path at `0x8233E4C0` binds null
through both setters.

## Runtime integration

The existing `SnapshotShadowState` now reads both device fields at each
configured draw boundary:

- `guest_vertex_shader = PPC_LOAD_U32(device + 0x3190)`;
- `guest_pixel_shader = PPC_LOAD_U32(device + 0x318C)`.

The two guest addresses are carried by the existing `ShadowState`, copied into
the existing `DrawCallRecord`, propagated to `RenderEventSignature`, and mixed
into its stable ID. The last captured draw remains the frame-level source, with
the established frame-end fallback when no draw is present.

The existing `active_vertex_shader_hash` and `active_pixel_shader_hash` remain
separate backend/swap metadata. No guest object pointer is presented as a
microcode hash, ShaderDef identity, material identity or parity proof.

No generated source, recompilation configuration, shader cache, renderer rule
or second trace sink was modified.

## Validation

- `VerifyShaderBindContracts.java`: **25/25** exact PPC assertions;
- `VerifyVertexDeclarationBindContracts.java`: **38/38**;
- `VerifyVertexDeclarationContracts.java`: **29/29** index-buffer assertions;
- standalone Clang 21 C++23 bridge test: **PASS**;
- strict `-fsyntax-only -Wall -Wextra -Werror` for the three affected sources:
  **PASS**;
- native AC6 build with `-j16`: **PASS**;
- native AC6 CTest: **44/44 PASS** in 33.46 s.

The bridge test covers last-draw selection, no-draw fallback, both guest shader
fields, two streams, null index fallback, and verifies that changing only the
guest pixel shader changes the stable signature.

## Open boundary

This cycle closes exact per-draw **guest shader-object identity** only. It does
not yet close:

```text
MATE identity -> technique/pass/permutation -> shader definition/microcode
              -> guest shader object -> draw
```

The next autonomous step is to derive a bounded ShaderDef or microcode identity
from the two captured guest objects, then trace the paired owner at
`0x82350318` back toward the MATE technique/pass/permutation selector. A Xenia,
VNC or human session is not required for that static work.

## Commands

```text
analyzeHeadless ... -postScript FindPpcMemoryDisplacement.java \
  0x82000000 0x82480000 0x318c
analyzeHeadless ... -postScript FindPpcMemoryDisplacement.java \
  0x82000000 0x82480000 0x3190
analyzeHeadless ... -postScript VerifyShaderBindContracts.java \
  -postScript VerifyVertexDeclarationBindContracts.java \
  -postScript VerifyVertexDeclarationContracts.java
clang++-21 -std=c++23 -Wall -Wextra -Werror ... \
  ac6_backend_capture_bridge.cpp ac6_backend_capture_bridge_test.cpp
clang++-21 -std=c++23 -fsyntax-only -Wall -Wextra -Werror ... \
  d3d_hooks.cpp ac6_backend_capture_bridge.cpp \
  ac6_backend_capture_bridge_test.cpp
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
```
