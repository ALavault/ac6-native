# AC6 retail NTSC-U/J — correction statique Gate 2

Date: 2026-08-30.

Cette tranche corrige les frontières natives vérifiables sans prétendre fermer
le runtime retail. Le PM4 natif utilise maintenant les opcodes Xenos réels
(`DRAW_INDX=0x22`, `WAIT_FOR_IDLE=0x26`, `WAIT_REG_MEM=0x3C`, `SET_CONSTANT=0x2D`,
`XE_SWAP=0x64`, `INDIRECT_BUFFER=0x3F`), les primitives Xenos valides, le format TYPE1 à deux registres,
le prédicat TYPE3, le fourcc `SWAP` et les capacités texture/blend/scissor/
primitive-restart. Les tests Release compilent avec
`-UNDEBUG`; l'ABI expose 128 registres VMX, les réservations PPC vérifient
l'alignement/invalidation et publient CR0, et les saves résolvent les noms
insensiblement à la casse.

## Validation déterministe

- `pytest -q recompilation/ace-combat-6-retail/tests`: **113 passed**;
- `tools/build.py --target ntsc-uj --profile native`: **CTest 5/5**;
- `tools/validate.py --target ntsc-uj --runtime native`: **pass**;
- `tools/census_native_imports.py`: **229** unique import names from the
  qualified US mapping; network class is recorded as offline-error/no socket
  creation. This is a census only, not a native binding.
- capsule `ac6.xenos-capsule.v1`: PM4 hardware borné, 7 dwords, validation
  pass, aucun octet retail.
- `--require-release` reste volontairement en échec avec le message stable
  `native full release gate is still open`.

## Génération CPU directe

Le projet Ghidra US canonique a exporté 8 163 fonctions sous l'identité XEX
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Une génération bornée avec toutes les frontières a atteint 20 minutes et a été
arrêtée par le timeout à 9,7 Go RSS. Une seconde génération a sélectionné les
89 propriétaires des diagnostics switch et s'est terminée avec 2 881
diagnostics (exit 2). Une variante croisant leurs débuts avec le mapping
XenonRecomp précédent a ensuite atteint le timeout de 20 minutes (exit 124,
sans receipt de succès). Ces sorties restent dans `build/` ignoré et ne sont
ni compilées ni liées.

Le gate reste donc ouvert : aucun boot/gameplay M01, aucun census/import binding
des 229 imports et aucune qualification campagne/save/release ne sont promus.
