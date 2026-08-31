# AC6 retail NTSC-U/J — génération CPU Gate 2 (ouverte)

Date: 2026-08-30.

## Exécution bornée

Une génération unique a utilisé le XEX US qualifié et les outils épinglés:

- XenonAnalyse: `ddd128bcca99fe8bfbb99bea583c972351fa6ace`, exit 0;
- XenonRecomp: même révision, exit 0;
- XEX: `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`;
- sortie: `build/ntsc-uj/native/codegen-20260830/` ignorée;
- wrapper: cgroup `ac6-retail-native-codegen-gate2`, exit 2 car diagnostics
  ouverts.

Le générateur a produit 83 fichiers / 61 075 947 octets, mais le reçu
`codegen-receipt.json` est `status=open-diagnostics`:

- 1 832 erreurs de flux `switch` hors fonction;
- 7 instructions non reconnues: `dcbst` ×4, `mulhdu` ×1, `frsqrte` ×2;
- 1 839 diagnostics au total.

## Décision fail-closed

La sortie générée n'est pas compilée ni liée au produit. Elle reste un artefact
de travail ignoré, conforme à la règle « généré = preuve seulement ». Le
runtime natif conserve son ABI handwritten et ne revendique aucun boot retail.

La cause immédiate est statique: la configuration directe manque encore les
helpers ABI `__save/__restgpr`, et les tables de saut XenonAnalyse ne sont pas
réconciliées avec les frontières de fonctions US qualifiées. Prochaine action:
qualifier ces frontières depuis le projet Ghidra US, puis relancer une seule
génération propre. Aucun A/B ni runtime avant diagnostics nuls.
