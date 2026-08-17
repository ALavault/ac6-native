# Cycle 1763 — audit d’intégration de `origin/infos`

## Résultat

La branche `infos` (`e6362d1e`) a été récupérée et intégrée uniquement pour ses
neuf fichiers d’analyse. Son diff depuis `883a58cd` ne contient aucun code.
L’ancien historique de code n’est donc ni repris comme preuve ni utilisé pour
la reconstruction.

## Identité imposée

| Champ | Valeur |
|---|---|
| cible | `ac6-demo-xbox360-pal` |
| module | `Default.xex` |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| projet Ghidra | `ace-combat-6-demo` |
| manifeste Ghidra SHA-256 | `576fa31e02b1c899cdc997b8a6e252d6d7785656d13067a9d8a54aeb2810086c` |

Les documents amont ne portaient pas cette identité complète. Le reçu
[`ac6-demo-infos-branch-audit-v1.json`](../analysis/ac6-demo-infos-branch-audit-v1.json)
scelle donc les hashes des neuf sources et leur requalification locale.

## Claims statiques corroborés

- `0x82273470` : les bytes PAL confirment `modifier 10`, le load
  `WeaponBin+0x5D`, la conversion `u8 → float` et l’addition dans `f1` ;
- `0x821E2F60` : la magnitude entrante est stockée dans le record compact à
  `+0x0C`, avec les champs `+0x00/+0x04/+0x08/+0x18/+0x1C` observables ;
- `0x8224FE60` : le code d’événement sélectionne un coefficient `DurableBin`,
  puis calcule `durabilité courante - coefficient × magnitude` à `+0x110` ;
- l’atlas PAL confirme les fonctions et appels principaux `0x822735A8`,
  `0x822A4978`, `0x82286830`, `0x822885C8`, `0x821E9550`, `0x821E2F60` et
  `0x821E23B8`.

Ce résultat qualifie un contrat **statique**, pas une transition runtime
endogène.

## Informations conservées mais non promues

- les hashes de WeaponBin/ManeuverBin, comptes d’objets et métriques Strigon ne
  donnent ni ranges de fichiers sources ni reçu d’extracteur ;
- les valeurs nominales `gun=8` et `missile=40` restent à rattacher à une
  capsule de records qualifiée ;
- `0x8216B258` et `0x8216B3A8` existent dans l’atlas, mais leur décompilation
  est indisponible : le transport missile complet reste partiel ;
- la priorité `ActBin / OrderBin / SetBin` est une suggestion de recherche et
  ne remplace pas le handoff courant ;
- aucun claim runtime, frontend, mission, terminal ou `supported=true` n’est
  ajouté.

## Décision d’intégration

| Groupe | Décision |
|---|---|
| contrat statique `WeaponBin+0x5D + modifier[10]` | intégré, qualifié PAL |
| record compact et équation DurableBin | intégrés, qualifiés statiques |
| transport missile complet | intégré comme partiel |
| profils et métriques Strigon | intégrés comme information seulement |
| prochain sous-système proposé | conservé comme advisory |
| gates runtime/frontend/mission | inchangés, NO-GO |

`supported=false` est maintenu.
