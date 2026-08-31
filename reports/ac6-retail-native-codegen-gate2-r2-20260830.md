# AC6 retail NTSC-U/J — génération CPU Gate 2 r2

Date: 2026-08-30.

La seconde génération directe réutilise le même XEX, outils épinglés et
contexte, avec huit helpers ABI qualifiés par signatures big-endian du
basefile US:

```text
savegprlr_14  0x823829E0    restgprlr_14  0x82382A30
savefpr_14    0x82383F30    restfpr_14    0x82383F7C
savevmx_14    0x82385150    restvmx_14    0x823853E8
savevmx_64    0x823851E4    restvmx_64    0x8238547C
```

XenonAnalyse et XenonRecomp terminent dans le cgroup borné, mais le receipt
reste `open-diagnostics` et le wrapper retourne 2:

- 1 824 erreurs de flux switch hors frontières;
- 7 instructions non reconnues (`dcbst` ×4, `mulhdu` ×1, `frsqrte` ×2);
- 1 831 diagnostics, 84 fichiers / 61 767 994 octets générés.

Les huit erreurs « helper address unspecified » de r1 ont disparu. Les 84
fichiers restent dans `build/ntsc-uj/native/codegen-20260830-r2/`, jamais dans
la lane native compilée. La prochaine action est une qualification Ghidra US
des frontières switch restantes; aucune modification de XenonRecomp ni
inclusion de sortie générée n'est autorisée.
