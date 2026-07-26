# Cycle 145 — correction de dispatch NDXR entre objet extérieur et sous-objet

## Cible et portée

- Target : `ac6-xbox360-pal-default-xex`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Analyse : Ghidra headless en lecture seule, sans session humaine.

Cette passe corrige une ambiguïté de vtable relevée dans les cycles 142–144.
Elle ne modifie ni le XEX, ni le projet Ghidra, ni une sortie générée.

## Distinction des deux tables

Le chemin `0x8212a2a8` écrit une vtable distincte à l'objet extérieur
(`0x8205d6c0`), puis appelle `0x820f9dc8` avec `outer+0x14`. Le corps de
`0x820f9dc8` écrit `0x8205c980` à l'offset zéro de ce sous-objet.

Le dump de `0x8205d6c0` montre notamment :

- `+0x00 -> 0x82127038` ;
- `+0x5c -> 0x820731bc`, une zone non désassemblée dans ce contexte, et non
  `0x82101be0` ;
- la table contient ensuite des données et chaînes, donc elle ne doit pas être
  fusionnée avec l'interface du sous-objet.

La table `0x8205c980` reste la seule table statiquement cohérente avec le
worker pour les instances initialisées par `0x820f9dc8` :

- `+0x5c -> 0x82101be0` ;
- `+0x13c -> 0x821002f0` ;
- `+0x10c -> 0x820fbc28` ;
- `+0x110 -> 0x820fa9c0`.

## Chemin d'appel qualifié

Dans `0x820fa9c0`, le `this` conservé dans `r31` reçoit le résultat du lookup
de ressource à `+0x28`. Les deux appels statiques du worker (`0x820fbbd4` et
`0x820fcf3c`) sont dans cette famille de méthodes et transmettent un pointeur
de contexte en `r3`. Le worker charge ensuite `context+0x00` puis le slot
`+0x5c` avant `bctrl`.

Cela ferme la provenance statique de la **famille de sous-objet**, mais pas la
valeur dynamique de chaque instance : le worker extrait toujours
`(entry_word >> 16) & 0x1ff` dans `r4`, alors que la feuille candidate
`0x82101be0` lit `lhz 0x1c(r4)`. Cette contradiction ABI/encodage reste
ouverte. Il est interdit d'appeler `r4` un pointeur ou de déclarer
`0x82101be0` producteur confirmé de `r5` sans preuve complémentaire.

## Décision

- `KEEP` : séparation objet extérieur/sous-objet et qualification du lookup
  `+0x28` comme table de ressources.
- `KEEP_WITH_CLARIFICATION` : `0x8205c980` est une vtable de sous-objet
  observée et cohérente avec la famille de méthodes, pas une preuve de toutes
  les instances runtime.
- `OPEN` : encodage de `r4`, remplacement éventuel de vtable, et identité
  métier de la valeur retournée dans `r5`.
- Aucune action humaine n'est requise à ce stade : la frontière restante est
  une ambiguïté statique/runtime et non un blocage d'outil.

## Validation

- Commandes : `DumpDataWords.java`, `ReferencesTo.java`, `DumpRange.java`,
  `FindPpcBranchesTo.java` via `analyzeHeadless` en `-readOnly -noanalysis`.
- CTest AC6 : `41/41 PASS` après cette passe.
- `git diff --check` : PASS.

