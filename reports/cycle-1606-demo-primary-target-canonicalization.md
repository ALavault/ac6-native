# Cycle 1606 — canonisation de la démo comme cible primaire

## Résultat

La cible active exclusive est la démo PAL `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
dans Ghidra 12.1.2 / `PowerPC:BE:64:Xenon` avec le projet
`ghidra-projects/ace-combat-6-demo`.

Le PAL retail `acc302c1…bcde` reste une cible séparée et gelée. Son worktree
fortement modifié a été inventorié mais n'a pas été modifié par ce cycle.
Aucune trace, preuve Ghidra ou identité retail n'est fusionnée avec la démo.

## Gates réconciliés

`demo-playable-gate-v1` conserve `supported=false` et les six lanes ouvertes,
mais distingue maintenant les sous-gates déjà fermés : codegen strict et
reproductible, runtime/scheduler PPC atteint, services offline partiels,
replay par tick et processeur PM4 transactionnel. Les frontières réelles sont
le movie XAM appel par appel, les consumers frontend, la traduction
shader/fetch/tiling/resolve, les services et l'audio atteints, puis la mission
endogène.

Les comptes 213 unités, 414 ObjBin, 4 factions et 3 sous-missions restent des
contrôles structurels uniquement.

## Validation

- codegen OFF : build à jour, CTest **12/12** ;
- codegen ON : build à jour sans régénération puisque ses entrées sont
  inchangées, CTest **11/11** ;
- audits source, complexité et statut dérivé : pass dans les deux matrices ;
- aucun chemin propriétaire ou binaire interdit n'est suivi par Git.

La première exécution a détecté un test de preuve périmé : il attendait encore
`synchronous_effect_execution=false` et l'ancien inventaire de trois shaders,
alors que la preuve PM4 courante qualifie les effets transactionnels, cinq
chargements/quatre microcodes et 26 draws cumulés. Le test a été remis en
accord avec cette preuve tout en exigeant toujours `pixel_output=false` et
`qualified_frontend=false`, puis la matrice complète est repassée.

Ce rapport ne ferme aucune des six lanes et ne revendique ni frontend ni
mission jouable.
