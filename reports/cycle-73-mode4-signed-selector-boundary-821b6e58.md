# Cycle 73 — mode-4 signed selector guard `Function_821B6E58`

## Evidence

- target: AC6 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- module/address: Xbox 360 PPC `0x821B6E58`;
- source: headless export
  [`0x821b6e58__Function_821B6E58.json`](../export-catalog/functions/821b/0x821b6e58__Function_821B6E58.json).

The mode-4 branch first assigns `iVar1 = (int)param_1` and tests
`0xd < iVar1 && iVar1 < 0x2b`. That arithmetic interval is therefore signed
32-bit. The subsequent table path instead uses the unsigned low-word guard
`0xd < (param_1 & 0xffffffff)` before forcing out-of-range values to zero.

The prior native correction handled ordinary selectors 14..42 correctly, but
used an unsigned interval test. A selector with bit 31 set could therefore
incorrectly enter `selector + 0x75f` rather than selecting table entry zero.

## Native correction

`function_821b6e58_resource_id` now casts the selector to `int32_t` only for
the mode-4 interval. The table bounds remain unsigned, matching the two
different XEX guards. Tests retain the ordinary 14..42 values and add
`0x8000000e` and `0xffffffff`, both of which return table entry zero (`0x1f`).

## Validation

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-mission-resource-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure \
  -R '^ac6-mission-resource-tests$'
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

The targeted test passed **1/1** and the complete AC6 corpus passed **41/41**.
This is a static numeric-selector correction; resource lifetime, scene
activation, and Xenia behaviour remain `needs-dynamic-evidence`.
