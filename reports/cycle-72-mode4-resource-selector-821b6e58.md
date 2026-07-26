# Cycle 72 — correction mode-4 de `Function_821B6E58`

## Identité et preuve

- Cible : AC6 PAL `default.xex`, Xbox 360 PowerPC big-endian ; SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Fonction : `Function_821B6E58`, `0x821B6E58`.
- Export :
  `workspaces/ace-combat-6/export-catalog/functions/821b/0x821b6e58__Function_821B6E58.json`.

Dans la branche `runtime_mode == 4`, le XEX établit d'abord le garde signé
`selector > 0x0d && selector < 0x2b`. Le test imbriqué contient ensuite
`selector < 0x0e`; il est donc impossible pour toute valeur ayant franchi le
premier garde. Le seul retour atteignable pour chaque sélecteur 14..42 est :

```text
selector + 0x75f
```

La transcription native rabattait auparavant les sélecteurs 14..20 sur
`0x76d`. Cette valeur est correcte pour le seul sélecteur 14, mais pas pour
15..20.

## Correction et tests

La branche native applique maintenant directement l'arithmétique attestée.
Les tests couvrent :

| selector | résultat XEX/natif |
|---:|---:|
| 14 | `0x76d` |
| 15 | `0x76e` |
| 20 | `0x773` |
| 21 | `0x774` |
| 42 | `0x789` |

## Validation

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-mission-resource-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure -R '^ac6-mission-resource-tests$'
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat : test ciblé **1/1**, corpus AC6 **41/41**, installation racine
réussie.

## Limite

Cette correction couvre uniquement le sélecteur numérique de ressources. Elle
ne prouve pas l'activation d'une scène, une mission jouable, ni une trace
Xenia; ces frontières runtime restent inchangées.
