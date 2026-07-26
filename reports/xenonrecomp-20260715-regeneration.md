# AC6 PAL XenonRecomp regeneration

- Target: `default.xex`, Xbox 360 Xenon/XEX, PAL, image base `0x82000000`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Route/status: `deterministic-fast-path` / `recompiler-generated`

## Reproducible result

`XenonAnalyse` regenerated 537 switch entries into a disposable file.  Its
byte-for-byte TOML output matched the established
`.tools/recomp-eval/ac6/ac6-switch-tables.toml` baseline.

`XenonRecomp` was then run with the existing AC6 configuration and
`XenonUtils/ppc_context.h`.  It emitted 80 `ppc_recomp.*.cpp` units and support
headers.  All 80 generated units passed C++20 syntax validation with the local
SIMDe include root.  The generated leaf `sub_8233EF48` corresponds to the
already separately verified native NDXR-signature helper; the generated code
was used as an instruction-level evidence source only.

## Boundary

The generation exits successfully but is not executable evidence: it reports
1,824 switch-label/function-boundary diagnostics and 55 unsupported
instructions, including `lhbrx`, `mulhdu`, and `frsqrte`.  It also lacks a
native Xenon/Xenos runtime and xboxkrnl/XAM/XMA services, with unresolved
indirect calls.  No generated file is compiled into `reconstruction/ace-combat-6`
or the Linux executable, and none was manually edited.
