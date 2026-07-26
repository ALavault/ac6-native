# AC6 — qualification de provenance du champ `+0x30` (cycle 156)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe reste headless et en lecture seule. Elle compare l'initialiseur
`0x820f9dc8`, le corps de méthode autour de `0x820fa9c0` qui appelle le worker,
la table `0x8205c980` et les candidats d'accès aux offsets `0x28`, `0x30`,
`0x5c` et `0x74`. Aucun offset numérique n'est considéré comme une preuve
d'identité d'objet.

Commandes principales :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x820f9dc8 0x820fa200 \
  -postScript DumpRange.java 0x820fa9c0 0x820fbe40 \
  -postScript DumpDataWords.java 0x8205c980 96 \
  -postScript FindFunctionsWithScalarSet.java 0x28 0x30 0x5c 0x74
```

## Deux chemins à ne pas fusionner

### Initialiseur `0x820f9dc8`

Le chemin observé à `0x820f9dc8` :

- écrit `0x8205c980` au premier mot de l'objet reçu ;
- initialise une série de champs flottants, dont `+0x28`, `+0x2c` et `+0x30` ;
- retourne après cette initialisation.

Ses appels directs retrouvés sont `0x8212a2a8` et `0x82183a28`. Cette preuve
ne suffit pas à dire que l'objet reçu est celui qui passe ensuite au worker.

### Méthode autour de `0x820fa9c0`

Le corps qui prépare l'appel `0x820fbbd4 -> 0x82105ba8` conserve un autre
receiver en `r31`. Dans ce chemin, il :

- remet à zéro plusieurs champs du receiver, dont `+0x28` et `+0x5c` ;
- remplit ensuite `+0x28` et `+0x5c` avec les résultats de lookups de
  ressources (`0x820fac34` et `0x820fac48`) ;
- appelle le worker avec `r3=r31` ;
- écrit ensuite des champs de résultat autour de `+0x6d50..+0x6d5c`.

Le corps ne lit pas directement `+0x30` avant ou après le worker. Le worker,
lui, publie son pointeur à `context+0x30` comme établi au cycle 155.

## Vtable et limites de provenance

La table `0x8205c980` contient notamment :

```text
+0x10c -> 0x820fbc28
+0x110 -> 0x820fa9c0
+0x13c -> 0x821002f0
```

Cela établit une famille de slots cohérente avec les méthodes observées, mais
ne prouve pas que chaque receiver du worker a été construit par
`0x820f9dc8`. De même, le fait que l'initialiseur écrive un flottant à
`+0x30` ne permet pas de requalifier le pointeur publié par le worker comme un
flottant, ni de requalifier le flottant comme le même champ logique.

Le balayage des fonctions contenant simultanément les scalaires `0x28`, `0x30`,
`0x5c` et `0x74` produit des candidats dans plusieurs sous-systèmes. Aucun
lecteur direct n'est attribué au owner du worker sans preuve de receiver et de
vtable. Les candidats restent donc `cross-match` ou `unknown`.

## Décision de preuve

`confirmed` :

- le worker reçoit le receiver préparé par le chemin autour de `0x820fa9c0` ;
- ce receiver possède les tables de ressources à `+0x28` et `+0x5c` dans ce
  chemin ;
- le worker publie un pointeur de zone à `+0x30` ;
- les écritures de `0x820f9dc8` à `+0x30` sont une autre observation,
  flottante, dont la provenance d'objet n'est pas fusionnée.

`unknown` :

- le lecteur ultérieur du pointeur `context+0x30` ;
- la durée de vie et le destructeur de cette zone ;
- la relation dynamique entre les receivers du worker et l'initialiseur
  `0x820f9dc8` ;
- la signification métier des valeurs quantifiées.

## Suite

La prochaine recherche peut cibler les slots de vtable qui lisent le pointeur
publié, mais elle doit d'abord résoudre les fonctions brutes non reconnues par
le catalogue `-noanalysis`. Une session humaine ou Xenia n'est pas encore
nécessaire : la limite actuelle est une provenance statique incomplète, pas un
outil manquant.
