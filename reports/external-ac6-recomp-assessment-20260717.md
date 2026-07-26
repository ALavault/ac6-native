# External AC6_recomp assessment — 2026-07-17

## Source inspected

Repository: <https://github.com/sal063/AC6_recomp>

The renderer/runtime follow-up is pinned locally at
`c5b089fb6988ac504ba394db611543bda2fb2c96` under
`.tools/ac6-recomp-reference`. It remains an external BSD-3-Clause reference;
no generated source or retail input is imported into the native product.

The upstream README describes a source-only, work-in-progress static
recompilation of AC6 using the ReXGlue SDK. It claims an x86-64 native port,
native D3D12/Vulkan rendering, and that the current build can boot and run
in-game while warning that crashes and missing functionality remain. The
repository contains no retail data or generated code; users supply their own
`default.xex`, and code generation writes a local `generated/` tree.

## Relevance to this repository

### High-value references

- ReXGlue's generated-code/runtime boundary may provide a second implementation
  to compare against XenonRecomp for PPC ABI helpers, guest-memory access,
  kernel stubs and host/guest lifecycle.
- The upstream native renderer layout is directly relevant to our open AC6
  graphics frontier: Xenos-to-native command translation, a render graph/frame
  plan, frame scheduling and separate Vulkan/D3D12 backends.
- Its Linux CMake presets and Clang-based build are a useful portability
  reference for the eventual full native executable.
- Its source-only/provenance policy is compatible with this repository's ban on
  retail data and generated-output edits.

### Important incompatibilities and unknowns

- Our qualified retail input is PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
  `AC6_recomp` does not publish the SHA-256, region or generated-code manifest
  of the XEX used by its current branch. The two projects cannot be assumed to
  target the same build.
- ReXGlue is a second recompilation toolchain, not an evidence source that
  replaces our Ghidra/XenonRecomp/XenonAnalyse/XenosRecomp records. Its output
  must be treated as `cross-match` until binary identity and behavior agree.
- The upstream README does not state a region for the input XEX. That omission
  does not reduce the value of its renderer/runtime architecture, but it does
  prevent treating generated addresses or behavior as a binary-qualified
  match for our PAL artifact. The README's codegen step reads the local
  `default.xex` and regenerates `generated/` when that input changes, so the
  input identity remains the comparison key even when region is not the useful
  architectural discriminator.
- The upstream README's boot/in-game statement is a project claim, not a
  differential proof for our PAL binary. It does not close our post-CUT,
  campaign, aircraft-owner or flight-input blockers.
- The generated tree is deliberately absent upstream, so there is no code
  corpus to copy safely without running code generation on a legally obtained,
  identity-matched XEX.

## Recommended use

1. Keep XenonRecomp and the existing Ghidra/Xenia evidence as the canonical
   control path.
2. Pin an upstream commit and inspect its exact ReXGlue config and codegen
   manifest in an isolated workspace.
3. If the same PAL XEX is accepted, generate a small, non-distributed corpus
   (ABI leaves, imports, guest-memory helpers and a few renderer paths).
4. Compare generated code and bounded behavior against our existing
   XenonRecomp/native tests and Xenia/Wine observations.
5. Reuse only independently licensed, portable runtime or renderer ideas that
   improve a measured gate. Do not merge a second orchestrator or symbol base.

The first useful experiment is therefore a binary-identity and ABI/rendering
comparison, not a migration. If the XEX differs, the project still remains a
valuable architectural reference and possible source of cross-match clues, but
its generated addresses and semantics must not enter the PAL evidence store.

## Executed shader-identity cross-match

The pinned ReXGlue renderer computes `Shader::ucode_data_hash()` with unseeded
XXH3-64 over the exact raw guest ucode byte span supplied to `LoadShader`.
The native retail scanner now reproduces that contract from each bounded
Xenos container (`virtual_size + Shader::physicalOffset`, `Shader::size`
bytes). Across all 3,808 shaders in the qualified PAL DATA corpus:

- `0x7d22894002d16018` matches entry 163/member 47,
  `ReductionBufferHalfRes.updb`, pixel stage, `0xc0` ucode bytes;
- `0x17e5e4ac3e713245` matches entry 163/member 47,
  `ReductionBufferSynthesisPS.updb`, pixel stage, `0x438` ucode bytes;
- `0xc049a8c9e556f129` and `0x2e372ea28cc404b7`, used by external trail
  diagnostics, have no match in the 3,808-container PAL corpus.

The two exact content matches independently validate the static-to-runtime
hash bridge without establishing whole-XEX identity. The two absences remain
build, runtime-generated or archive-scope differences; they must not be forced
onto a retail PAL shader name.

## Licensing/provenance

The GitHub repository advertises a BSD-3-Clause license and explicitly forbids
redistributing `default.xex`, disc images, packages, firmware or keys. Verify
the exact license of any ReXGlue component before copying code, preserve
attribution, and keep all generated code and retail inputs local and ignored.
