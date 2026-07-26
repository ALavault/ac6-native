# Cycle 74 — largeur complète du sélecteur `Function_821B6E58`

## Evidence

- target : AC6 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- module/adresse : Xbox 360 PPC `0x821B6E58` ;
- source : export headless
  [`0x821b6e58__Function_821B6E58.json`](../export-catalog/functions/821b/0x821b6e58__Function_821B6E58.json).

L'export déclare `ulonglong Function_821B6E58(ulonglong param_1)`. Ses chemins
de tables bornent et indexent le mot bas de `param_1`, mais deux chemins
arithmétiques retournent directement `param_1 + constante` : mode 4 pour les
valeurs signées 14..42, et modes 2/5 lorsque `FUN_820f61b0` vaut 6 avec un mot
bas dans 1..15. Les bits hauts restent donc observables dans ces deux retours.

## Native boundary

`function_821b6e58_retail_value` conserve maintenant cette largeur `uint64_t`.
Le paramètre `secondary_mode` reste injecté : il représente le résultat de
`FUN_820f61b0`, dont le propriétaire runtime n'est pas encore reconstruit.

L'ancienne API `function_821b6e58_resource_id` est conservée comme frontière
de catalogue 32 bits. Elle appelle la primitive retail puis réduit explicitement
le résultat, ce qui préserve les consommateurs actuels de DATA.TBL sans les
faire passer pour une ABI XEX complète.

Les tests couvrent :

- `0x000000010000000e`, mode 4 : `0x000000010000076d` ;
- `0x0000000100000001`, mode 2 / secondaire 6 :
  `0x000000010000075e` ;
- un chemin de table avec mot bas 0, qui retourne bien l'entrée 32 bits
  `0x1f` après son garde-fou.

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

Le test ciblé passe **1/1** et le corpus AC6 complet **41/41**.

Cette correction ne prouve pas la propriété du runtime, le chargement d'une
scène, ni le comportement Xenia ; ces frontières restent
`needs-dynamic-evidence`.
