# Third-party provenance

## XenonRecomp

The optional code-generation step uses XenonRecomp at commit
`ddd128bcca99fe8bfbb99bea583c972351fa6ace`, recorded together with its
submodule revisions in `config/xenonrecomp.lock.json`.

The checkout is fetched into the build directory, patched there, and never
copied into the source product. The patch files under `patches/` are the
reproducible AC6 strict-mode integration. `tools/ppc_context_adapter.h` is a
small build-only adapter around the generator's public context shape; it does
not contain game-generated code.

The upstream license is retained in the fetched checkout as `LICENSE.md`.
Redistribution of the generated output is intentionally excluded by the
product's `.gitignore` and source-package rules.

## ReXGlue shader translator reference

The raw Xenos shader boundary was cross-checked against the ReXGlue shader
analyser at commit `dcd41b7457fcac8242f8ef40de83d1719390d5af`, specifically
`src/graphics/pipeline/shader/translator.cpp` (SHA-256
`448576e802b52019481f58168bc82f6192b426493870491a4db7e394410aac10`).
That reference is BSD-3-Clause, copyright 2015 Ben Vanik and Xenia project
contributors, modified by Tom Clay in 2026. Its license file has SHA-256
`8e065be1da2ff9a16b1f063d4636d8b67e6a654bb90583ea4332e66ac421bb18`.

No ReXGlue opcode-unpacking or shader-emission source is copied into this
product. The current implementation only preserves the independently observed
three-dword envelope and refuses semantic translation until the AC6 demo
opcode and fetch inputs are qualified.

The refusal boundary was also checked against XenosRecomp commit
`990d03b28a27b50277ee5d8d942e1c5f873869d1` (MIT, copyright 2025 hedge-dev
and contributors). Its README, SHA-256
`56a7c5074c166377554822b2812830d4f889844c39c10ae89ed5998b29a8f5e1`,
requires container reflection data and records mini vertex fetches as
unimplemented. Its license SHA-256 is
`bcb33ae2cbc6c0f2818712d806112e774e684fce01ce80573f92adf80b7487f4`.
XenosRecomp is neither copied nor linked by the product.
