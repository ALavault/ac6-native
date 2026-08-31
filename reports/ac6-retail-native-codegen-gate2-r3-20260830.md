# AC6 retail NTSC-U/J — génération CPU Gate 2 r3

Date: 2026-08-30.

Le wrapper `tools/generate_native_guest.py` a validé les huit helpers ABI avant
exécution puis a lancé XenonAnalyse/XenonRecomp dans un cgroup borné. Receipt:
`build/ntsc-uj/native/codegen-20260830-r3/codegen-receipt.json`.

```text
XenonAnalyse: exit 0
XenonRecomp: exit 0
generated: 84 files / 61 767 994 bytes
status: open-diagnostics
diagnostics: 1 831 (1 824 switch-boundary, 7 unrecognized)
wrapper exit: 2
```

Les helpers reçus sont `savegprlr_14=0x823829E0`,
`restgprlr_14=0x82382A30`, `savefpr_14=0x82383F30`,
`restfpr_14=0x82383F7C`, `savevmx_14=0x82385150`,
`restvmx_14=0x823853E8`, `savevmx_64=0x823851E4` et
`restvmx_64=0x8238547C`. La sortie reste ignorée, non compilée et non liée;
les frontières switch US et sept opcodes restent à qualifier.

La preuve machine-readable est `artifacts/retail-us-native-codegen-gate2/helpers.json`,
produite par `tools/qualify_xenon_helpers.py`; chaque signature apparaît une
seule fois et sur frontière dword.
