# AC6 cycle 256 — bounded guest shader-program references

Date: 2026-07-19

## Question

Can the guest shader objects captured in cycle 255 yield a stronger,
content-location identity without treating an allocation pointer as a shader
hash or claiming a vertex variant that the current hook cannot observe?

## Qualified identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- image base: `0x82000000`;
- Ghidra project: canonical `ace-combat-6`, read-only and headless.

## Recovered formulas

The compact/pixel object loaded from `device+0x318C` has one program reference:

```text
descriptor = object + u32be(object + 0x40)
program    = u32be(object + 0x18) + u32be(descriptor + 0x28)
size_bytes = u32be(descriptor + 0x2C)
```

The common pre-draw emits `size_bytes >> 2` into the Xenos packet. The native
snapshot preserves the unshifted byte count.

The vertex object loaded from `device+0x3190` uses an eight-byte-stride variant
table:

```text
descriptor(i) = object + u32be(object + 0x380 + 8*i)
program(i)    = u32be(object + 0x20) + u32be(descriptor(i) + 0x368)
size_bytes(i) = u32be(descriptor(i) + 0x36C)
```

Within physical function `0x821ED1D0`, the concrete variant values assigned to
the upload registers are only `0` and `1`; `-1` means no upload. The snapshot
therefore carries the two bounded candidates `i=0,1`.

## Capture-order boundary

The configured draw chunks run before the call to `0x821ED1D0`:

- indexed draw chunk `0x821DEF18`, compiler call `0x821DEF60`;
- shared indexed draw chunk `0x821DF300`, compiler call `0x821DF34C`.

Consequently the current hook cannot truthfully identify which vertex variant
won for that draw. The new fields are deliberately named
`vertex_shader_program_candidates`; they are not selected-program fields. The
pixel reference is singular. None of these address/extent pairs is called a
content hash or parity proof.

## Runtime integration

`SnapshotShadowState` now recovers:

- one `pixel_shader_program` address/size pair;
- two `vertex_shader_program_candidates` address/size pairs.

They propagate through the existing draw record and `RenderEventSignature` and
participate in its stable ID. Null shader objects clear their derived fields.
The existing guest object pointers and backend swap shader hashes remain
separate.

No program bytes are dereferenced or hashed, no generated/config file is
changed, and no second capture sink is introduced.

## Validation

- `VerifyShaderProgramReferenceContracts.java`: **31/31** exact PPC assertions;
- `VerifyShaderBindContracts.java`: **25/25**;
- vertex-declaration assertions: **38/38**;
- index-buffer assertions: **29/29**;
- standalone Clang 21 C++23 bridge executable: **PASS**;
- strict syntax check with warnings as errors for three affected sources:
  **PASS**;
- native AC6 build with `-j16`: **PASS**;
- native AC6 CTest: **44/44 PASS** in 32.94 s.

The bridge test proves last-draw and no-draw fallback propagation and changes
the stable ID by changing only one candidate program address.

## Open boundary

This cycle closes bounded **program address/extent identity**, not microcode
content hashing and not the selected vertex variant. A selected-variant claim
requires a qualified capture boundary after `0x821ED1D0`, or another proven
persisted field. The next static task is to trace the paired owner at
`0x82350318` back to one MATE technique/pass/permutation family and determine
whether a minimal post-compiler hook is justified.

No Xenia, VNC, GUI or human session is required for that work.

## Commands

```text
analyzeHeadless ... -postScript VerifyShaderProgramReferenceContracts.java \
  -postScript VerifyShaderBindContracts.java \
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
