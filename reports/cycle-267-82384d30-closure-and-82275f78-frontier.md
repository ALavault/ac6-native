# AC6 cycle 267 — fermeture `0x82384D30` et front runtime `0x82275F78`

## Identité et archive reçue

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- archive : `ac6_82384d30_boundary_audit_v1.zip` ;
- SHA-256 archive :
  `3888a725e4b507b5fadacda8e75ce0e9de3bec487944869771c377eef31efb94`.

L'archive passe `unzip -t`; ses dix charges utiles passent son
`SHA256SUMS`. Les six tests exécutables localement passent et un test dépendant
de `AC6_INPUT_ROOT` est sauté. Le patch reproduit localement est identique à
celui de l'archive.

## Limites appliquées

L'audit classe comme départs internes confirmés `0x82384D08`, `0x82384D88`
et `0x82384E48`. Il conserve les entrées réelles `0x82384CE8`, `0x82384E10`
et `0x82384E34`, et laisse `0x82384CE4` ambiguë. La fonction contenante
qualifiée est `0x82384CE8..0x82384E0F`.

Seules les trois entrées internes ont été retirées de
`.tools/ac6-recomp-reference/ac6recomp_config.toml`. Le C++ généré n'a pas été
modifié manuellement. Le hash du TOML après patch est exactement celui attendu :

```text
a3682cd89d4ff582d3516c9d6ee23888e6b3d426c5f5d67d30166aaf438d6a1e
```

## Régénération et build

```bash
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp_codegen -j16
cmake --build .tools/ac6-recomp-reference/out/build/linux-amd64-runtime-localdev \
  --target ac6recomp -j16
```

Les deux commandes passent. La nouvelle unité
`generated/ac6recomp_recomp.41.cpp` a le SHA-256
`df14c2459e82aa1e4d39eb32194010f6e68a25290318382ecfb7fb66bc7f5f27`.
Elle ne contient plus le fatal `0x82384DE4 -> 0x82384D30`, ni de symbole
généré pour les trois départs retirés.

## Smoke borné et nouveau front

Un premier lancement sans assets échoue proprement dans `SetupVfs`. Le
répertoire retail qualifié a ensuite été monté comme `assets` à côté du
binaire construit, sans copier les PAC. Le smoke atteint le code PPC recompilé
puis s'arrête avec le nouveau fatal exact :

```text
Unresolved branch from 0x82275FB4 to 0x82275F78
```

Le processus termine par `SIGABRT` (`134`). Il n'a demandé aucune action
humaine. Ce fatal, et non la famille `0x82384D30`, est maintenant le premier
front runtime observé.

Le C++ généré montre une fonction à `0x82275F60`, une tête de boucle à
`0x82275F78`, la branche arrière à `0x82275FB4`, puis l'épilogue jusqu'à
`0x82275FC0`. La configuration contient encore `0x82275F88`, qui tombe dans le
corps de cette boucle. `0x82275FC8` possède un nouveau prologue autonome.

L'export headless frais `0x82275F20..0x82276010` confirme les instructions et
la branche arrière. Ghidra ne matérialise toutefois que les stubs initiaux
`0x82275F60..0x82275F67` et `0x82275FC8..0x82275FCF`, car les helpers de
sauvegarde/restauration perturbent ses limites. Le retrait de `0x82275F88`
reste donc à qualifier indépendamment avant de modifier le TOML.

## Frontière suivante

Faire auditer la famille bornée `0x82275F60..0x82275FC8`, avec comme question
unique le statut de l'entrée configurée `0x82275F88`. Aucun `DATA00.PAC` ou
`DATA01.PAC` n'est nécessaire pour cette question de flot de contrôle.

Une fois le verdict qualifié, appliquer au plus petit le patch TOML, régénérer
une fois, reconstruire et relancer le même smoke borné. Aucun generated C++ ne
doit être édité à la main.
