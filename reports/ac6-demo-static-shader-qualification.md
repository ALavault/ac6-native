# AC6 demo PAL — static shader qualification

## Result

The four reached renderer shaders can be qualified without starting the guest
and without replaying a PM4 capture.  They are exact `.rdata` ranges in the
qualified PAL basefile, not products that need to be recovered from a running
command buffer.

| Stage | Static guest range | Bytes | Microcode SHA-256 | SPIR-V SHA-256 |
|---|---|---:|---|---|
| VS | `0x82013E20..0x82013E7F` | 96 | `099625f3…21e4e3` | `944fd752…23ce6` |
| PS | `0x82013E80..0x82013EA3` | 36 | `4913603d…c98e25` | `f6422d60…ce9fe` |
| VS | `0x820140A0..0x8201410B` | 108 | `93488cb9…a0402b` | `ba9b97cc…b0576` |
| VS | `0x82014140..0x8201417B` | 60 | `586168ec…a83cc0` | `4913cadb…dc920` |

The canonical static atlas already contains direct source references from
functions `0x821B1D58`, `0x821B6078` and `0x821B6FD0`.  Their static PM4
producer call-shapes establish the immediate shader loads.  The pixel shader
also appears in two embedded NSXR containers at `0x8264B68C` and
`0x8264BA8C`.

## Offline gate

`validate_qualified_vertex_sources.py` now validates all four stages despite
its historical filename.  It performs the following fail-closed sequence:

1. require basefile SHA-256 `b98a9ac1…14218`;
2. slice the four exact ranges and require stage, size and SHA-256;
3. translate each shader with the pinned ReXGlue translator;
4. require the four golden SPIR-V sizes and hashes;
5. run the pinned `spirv-val` for Vulkan 1.1 with scalar-block layout;
6. keep microcode, disassembly and SPIR-V under `TMPDIR` only.

Result: `qualified_shader_sources=4/4`, `spirv_val=4/4`.

This gate qualifies shader identity and translation statically.  Runtime
observation remains useful only to prove that a particular route selects a
shader and to join it to draw state; it is no longer required to recover or
validate the shader itself.

## Extension to the PAC corpus

The same scheme applies to the 1,891 unique PAC microcodes: archive identity,
DATA.TBL index, FHM path, NSXR descriptor, stage, size, raw/swap32 hashes,
offline translation and `spirv-val`.  The metadata-only PAC inventory already
provides the source half of this gate.  Unknown opcodes or a failed translation
remain `unknown`; they are never approximated.

The durable receipt is
`analysis/demo/ac6-demo-static-reached-shaders-v1.json`.  No shader,
microcode, generated SPIR-V or proprietary container is tracked.
