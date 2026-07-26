# Cycle 229 — VMX transform runtime probe

## Identity and scope

- Target: AC6 Xbox 360 `default.xex`.
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Retail path: `0x82119620 -> 0x82118F10 -> 0x82119458`.
- Scope: the finite-input angle-to-quaternion transform used by the first
  dispatch wrapper. This is not whole-XEX runtime or gameplay parity.

## Deterministic blocker removal

The XenonRecomp disassembler already decoded `vandc` and `vsel128`, but its
recompiler switch did not emit them. The local checkout remains based on
`ddd128bcca99fe8bfbb99bea583c972351fa6ace`; the reproducible patch is:

`tools/patches/xenonrecomp/0001-support-vandc-vsel128.patch`

Patch SHA-256:
`bed65f5693f91c1be71174ce437a060b28ddaf053e8b08822189ee748a2ae28b`.

It covers exactly:

- `vandc`: `0x8211910C`, `0x821191B4`;
- `vsel128`: `0x821192FC`, `0x82119394`, `0x82119428`.

After rebuilding XenonRecomp and regenerating normally, the global unsupported
instruction count fell from 55 to 24. The generated output was not edited.
Remaining unsupported families elsewhere include `vpkswss`, `dcbst`, `lhbrx`,
`mulhdu`, and `frsqrte`.

## Retail constants and execution

The twelve immutable vectors read by `0x82118F10` were exported read-only from
the canonical Ghidra project. The qualified log is:

`workspaces/ace-combat-6/reports/cycle-229-vmx-constants.log`

The permanent Clang-only probe links the needed generated translation unit,
reserves the sparse 32-bit guest address space, installs those constants, and
executes the generated composition and matrix-to-quaternion leaf.

Analytic observations:

- zero angles -> `[1,0,0,0]`;
- X half-pi -> `[sqrt(1/2),0,0,sqrt(1/2)]`;
- Y half-pi -> `[sqrt(1/2),0,sqrt(1/2),0]`;
- Z half-pi -> `[sqrt(1/2),sqrt(1/2),0,0]`;
- changing only the fourth lane leaves the identity result.

The output layout is `[w,z,y,x]`; the first three lanes compose as
`qZ * qY * qX`.

## Differential candidate

The first 343-case run rejected the direct formula because some combined
rotations returned `-q` where retail returned `q`. Those values encode the same
orientation but are not raw-word parity and can affect interpolation. The
candidate now reproduces retail's trace/dominant-diagonal sign selection.

Final bounded grid:

- angles per axis: `-1.25, -0.75, -0.25, 0, 0.25, 0.75, 1.25` radians;
- 343 triplets;
- maximum component error: `1.19209e-07`;
- accepted tolerance: `1e-4`;
- result: 343/343 pass.

Non-finite values in the first three lanes fail closed. The fourth lane is
ignored by this path, including when it contains NaN.

## Validation

Clang generated-code probes:

```text
ctest --test-dir .build/ace-combat-6-clang-probes --output-on-failure \
  -R '^ac6-xenonrecomp-(first-dispatch-transform|ndxr-signature)-probe$'
2/2 passed
```

Native AC6 corpus:

```text
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
42/42 passed
cmake --install .build/ace-combat-6 --prefix "$PWD"
test ! -e bin/bin
passed
```

## Boundary

This validates one finite range and one helper composition. Huge finite angles,
floating-point exception details, NaN payload behavior, downstream quaternion
interpolation, and the remaining unsupported Xenon instructions are still open.
The function therefore remains an explicit callback rather than becoming an
unconditional implicit transform. No Xenia, GUI, VNC, or human action was
needed.
