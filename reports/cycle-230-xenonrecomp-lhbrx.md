# Cycle 230 - XenonRecomp `lhbrx`

Date: 2026-07-18

## Boundary

Target: AC6 retail Xbox 360 XEX, Xenon PowerPC big-endian. This cycle extends
the deterministic XenonRecomp evaluation only. It does not claim that the
generated whole-game runtime is runnable or behaviorally complete, and Xenia
was not launched.

## Evidence and implementation

The previous reference generation reported 24 unsupported instructions, of
which 15 were `lhbrx`. The upstream decoder already identifies
`PPC_INST_LHBRX`. The reproducible patch
`tools/patches/xenonrecomp/0002-support-lhbrx.patch`, applied after the
existing `0001-support-vandc-vsel128.patch`, adds the emission:

```cpp
rD.u64 = __builtin_bswap16(PPC_LOAD_U16(RA0 + RB));
```

`PPC_LOAD_U16` performs the normal big-endian guest load. The additional swap
implements the byte-reversed halfword result; assignment to `u64` zero-extends
it. The `RA=0` encoding omits the first addend, as required by indexed PPC
addressing. Generated output was regenerated normally and was not edited.

## Validation

```sh
cmake --build .tools/xenonrecomp-clang-build -j16 --target XenonRecomp
.tools/xenonrecomp-clang-build/XenonRecomp/XenonRecomp \
  .tools/recomp-eval/ac6/ac6.toml \
  .tools/xenonrecomp-source/XenonUtils/ppc_context.h
cmake --build .build/ace-combat-6-clang-probes -j16 \
  --target ac6-xenonrecomp-lhbrx-probe
ctest --test-dir .build/ace-combat-6-clang-probes --output-on-failure \
  -R '^ac6-xenonrecomp-lhbrx-probe$'
find .tools/recomp-eval/ac6/output -maxdepth 1 -name '*.cpp' -print0 | \
  xargs -0 -n1 -P16 clang++ -std=c++20 -fsyntax-only \
    -I.tools/recomp-eval/ac6/output \
    -I.tools/xenonrecomp-source/thirdparty/simde
```

Results:

- bounded `lhbrx` probe: 1/1 passed;
- semantics: `RA=0`, non-zero `RA+RB`, byte reversal, zero extension;
- retail sites: 15 comments and 15 matching emissions;
- syntax-only compilation: all 81 generated C++ files passed (80 numbered
  units plus the mapping unit);
- full Clang corpus including generated-code probes: 45/45 passed;
- normal GNU native corpus: 42/42 passed;
- unrecognized instructions: 24 -> 9;
- unrecognized `lhbrx`: 15 -> 0.

The direct-call reachability classifier remains unchanged: none of the 55
historical compatibility sites is in the currently proved direct visual-MVP
closure. This is a prioritization result, not a whole-program reachability
proof.

## Remaining boundary

The latest generator log still reports exactly nine unsupported instructions:
four `dcbst`, two `vpkswss`, one `mulhdu`, and two `frsqrte`. Switch-boundary
diagnostics, unresolved indirect calls, runtime services and the three existing
`vcmpbfp.` RC-bit warnings remain outside this cycle.
