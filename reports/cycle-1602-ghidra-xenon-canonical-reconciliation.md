# Cycle 1602 — réconciliation du projet Ghidra Xenon canonique

## Identité et périmètre

Cette opération est `static` uniquement. Elle ne modifie pas le runtime natif,
le renderer, Xenia ou les actifs retail suivis.

- cible : PAL Xbox 360 `default.xex` ; SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- `DATA.TBL` : SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5` ;
- projet actif : `ghidra-projects/ace-combat-6` ;
- Ghidra : 12.1.2, Java 21.0.11, loader XEX Loader by Warranty Voider ;
- langage : `PowerPC:BE:64:Xenon` ; image base `0x82000000` ; entrée
  `0x821F5E90`.

Le projet A2ALT précédent a été déplacé, sans suppression, vers
`ghidra-projects/historical-a2alt-20260814`. Aucun export des deux projets
n'est fusionné.

## Résultat reproductible

Le script versionné `tools/import_ac6_ghidra_xenon.py` refuse un projet existant,
vérifie les deux hashes d'entrée et la version Ghidra, puis importe avec
`-processor PowerPC:BE:64:Xenon`. `tools/ghidra/ExportOracleFunctionBoundaries.java`
refuse désormais A2ALT et normalise le suffixe optionnel `:default`.

Preuves :

- manifeste : `analysis/ghidra/canonical-import.json` ;
- journal résumé : `analysis/ghidra/canonical-import.log` ;
- export : `analysis/ghidra/canonical-function-boundaries.json` ;
- export SHA-256 :
  `fa3cf70e8acf811b2d273544f954078dcbf9577cf65f223af53ec1b29e59c40f`.

Le `ghidra-bridge export all` a été tenté après la promotion. Il a été arrêté
après un avertissement p-code répété sur `0x82073E3C`; les fichiers partiels
dans `exports/` sont ignorés et explicitement **non qualifiés**. L'export de
bornes Java ci-dessus reste la seule source statique de cette mise à jour.

Le corpus actif contient **10 708 fonctions**. L'export read-only de l'ancien
corpus A2ALT contenait 10 645 fonctions (`5e7119fa…284f1`). Le chargeur Xenon
déclare 8 246 fonctions `.pdata`; l'analyse active ajoute les bornes restantes.

## Décision

La mise à jour Ghidra change effectivement les résultats statiques :
frontières (+63 fonctions) et décodage VMX128. Les anciennes observations
bridge restent des faits historiques de leur runtime et doivent être
requalifiées avant toute promotion. Le prochain corridor est donc
`ObjBin -> WeaponBin/DurableBin -> loadout -> destruction -> compteur`, avec un
lot de hooks PPC bornés et une capsule bridge séparée.

Les fichiers documentaires `NATIVE_RECONSTRUCTION_STATUS.md`,
`DECOMPILATION_PLAN.md`, `CURRENT_PLAN.md` et `reports/handoff/CURRENT.json`
étaient absents de ce dépôt ; aucun plan d'un autre produit n'a été importé.
