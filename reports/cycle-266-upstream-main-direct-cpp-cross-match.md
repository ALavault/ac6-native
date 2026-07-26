# AC6 cycle 266 — direct C++ from upstream `AC6_recomp` main

## Question

Can current upstream `sal063/AC6_recomp` generate direct C++ from the qualified
PAL XEX, and does that output add useful evidence relative to the local
decompilation and corrected recompilation configuration?

## Qualified inputs

- target: `ac6-xbox360-pal`;
- module: `default.xex`;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- upstream checkout: `.tools/ac6-recomp-main-reference`;
- upstream commit: `dcd41b7457fcac8242f8ef40de83d1719390d5af` (`main`);
- local comparison checkout: `.tools/ac6-recomp-reference` at
  `c5b089fb6988ac504ba394db611543bda2fb2c96` with the qualified local function
  boundary corrections.

Both generated trees and the retail XEX remain ignored local artefacts. No
generated C++ or retail byte was copied into the reconstruction.

## Build and generation

The newly installed packages are visible as:

```text
libgtk-3-dev     3.24.52-0ubuntu1
libx11-xcb-dev   2:1.8.13-1
libvulkan-dev    1.4.341.0-1
clang            21.1.8
cmake            4.2.3
ninja            1.13.2
```

The clean upstream `main` vendors an incomplete SIMDe snapshot: its expected
`simde/x86/avx.h` is absent. The already present local `dev-test` checkout
contains that header. To avoid changing upstream source or installing another
copy, configuration used only that checkout's SIMDe include directory as a
read-only header fallback:

```bash
cmake --fresh --preset linux-amd64-relwithdebinfo \
  -DCMAKE_C_COMPILER=clang-21 \
  -DCMAKE_CXX_COMPILER=clang++-21 \
  -DCMAKE_CXX_FLAGS='-march=x86-64-v3 -I/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/thirdparty/rexglue-sdk/thirdparty/simde'

cmake --build --preset linux-amd64-relwithdebinfo \
  --target ac6recomp_codegen -j16
```

Configuration and code generation passed. The generator reported completion in
13.056 seconds after its toolchain had been built. The direct output is:

- `56` generated files;
- `52` C++ translation units;
- approximately `101 MiB`;
- `23,858` registered imports;
- `23,376` generated function implementations.

This was a code-generation validation, not a full runtime build or playability
test.

## Cross-match result

The direct C++ is a literal PPC-context translation. It preserves loads,
stores, condition-register tests, register widths and branch addresses, which
makes it useful beside Ghidra decompilation. It does not recover meaningful
classes, field names or source-level types by itself.

The most useful comparison is the current runtime frontier. Upstream `main`
still configures these entries:

```text
0x823849F0
0x82384AAC
0x82384AE8
0x82384AF0
```

Its generated `ac6recomp_recomp.41.cpp` emits separate implementations for the
first three and leaves:

```text
Unresolved branch from 0x82384AD0 to 0x82384A88
```

The same unit nevertheless contains `loc_82384A88` inside the containing body.
This independently reproduces the exact failure already qualified in cycle
265. The canonical headless Ghidra evidence instead gives:

```text
0x823849C8..0x82384AEF  one function
0x82384AF0..0x82384B2F  next function
```

The local corrected configuration therefore removes the three internal starts
and retains `0x82384AF0`. It contains 15 other qualified removals not yet
present on upstream `main`, for a total difference of 18 configured entries.
After a fresh local regeneration, the generated inventories reflect exactly
that delta:

| Inventory | upstream main | local corrected |
| --- | ---: | ---: |
| registered imports | 23,858 | 23,840 |
| implementations | 23,376 | 23,358 |
| unresolved-branch occurrences | 4,974 | 4,949 |
| unique unresolved branch pairs | 1,597 | 1,589 |

The unresolved counts are diagnostics, not a count of incorrect functions:
one bad configured boundary can produce repeated generated implementations and
several fatal sites.

Two existing renderer contracts also appear directly in the generated main
output and agree structurally with our Ghidra-derived work:

- `0x821DD068` consumes guest arguments in `r3..r8`, stores the resource into
  the device table indexed by `r4`, and updates device dirty/state words;
- `0x821DE5C0` stores the vertex-side object at device offset `+0x3190`
  (`12688`) and updates associated state flags.

These agreements are `cross-match`, not new semantic confirmation. The native
names and contracts remain grounded in the qualified XEX, headless Ghidra and
the targeted assertions from cycles 251–256.

## Boundary and next use

Upstream `main` successfully supplies the requested direct C++ comparison
corpus. It does not supersede the local corrected configuration: its current
output contains 4,974 unresolved-branch fatal occurrences and repeats the
known `0x82384AD0 -> 0x82384A88` defect.

The local corrected checkout was then regenerated once with:

```bash
cmake --build out/build/linux-amd64-runtime-localdev \
  --target ac6recomp_codegen -j16
```

It completed in 13.538 seconds. The generated output contains:

- zero occurrences of
  `Unresolved branch from 0x82384AD0 to 0x82384A88`;
- zero generated symbols for `0x823849F0`, `0x82384AAC` or `0x82384AE8`;
- one retained import for the real next function at `0x82384AF0`;
- SHA-256
  `a1688fd94383b7c9e7a6b24c2edeb3f3b229c337cba669f6db75b8ee0ee70b76`
  for the regenerated `ac6recomp_recomp.41.cpp`.

This closes the code-generation half of the cycle-265 criterion. It does not
establish the next runtime frontier because the retail smoke was not run.

The next efficient step is the bounded retail smoke already defined by cycle
265. Further direct C++ comparisons should select named addresses from the
renderer or runtime frontier rather than diffing the full 101 MiB generated
tree.
