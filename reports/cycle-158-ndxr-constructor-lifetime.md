# AC6 — initialisation et durée de vie statique du contexte NDXR (cycle 158)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

La passe est headless, en lecture seule et limitée à l'île de code autour de
`0x820fa258` (initialisation du receiver NDXR) et `0x82105bb8` (worker déjà
qualifié). Les dumps ont été exécutés avec `DumpRange.java` dans un répertoire
de scripts temporaire afin d'éviter le cache de compilation Ghidra périmé.

## Résultats

### Initialisation du receiver

Le chemin à partir de `0x820fa258` écrit le vtable address-point
`0x8205c9a4` à `receiver+0x0`, puis initialise notamment :

```text
receiver+0x28 = 0
receiver+0x2c = 0
receiver+0x30 = 0
receiver+0x34 = 0
receiver+0x38 = 0
receiver+0x5c = 0
receiver+0x60 = 0
receiver+0x70 = 0
receiver+0x74 = 0
receiver+0x78 = 0
```

Cette séquence confirme que `+0x30` est un champ initialement nul du receiver
qui possède le vtable NDXR. Elle ne suffit pas à lui attribuer un type C++ ni à
prouver que son contenu publié par `0x82106344` est consommé par les slots
`+0x7c/+0x80/+0x84`.

### Worker et publication

Le worker à `0x82105bb8` conserve `r3` dans `r18`, utilise `context+0x28` et
`context+0x5c` avant ses parcours, puis écrit `stw r25,0x30(r18)` à
`0x82106344`. Les valeurs `+0x28` et `+0x5c` sont donc des entrées de travail
distinctes du champ publié `+0x30`.

Le même worker effectue des dispatchs indirects via le vtable (`lwz
r10,0x5c(vptr); bctrl`) et appelle `0x822c2148`; aucune référence directe
au worker `0x82105bb8` n'est retrouvée par les balayages de branches absolues
et relatives. Cela laisse ouverte l'hypothèse d'une entrée par pointeur de
fonction, table ou thunk non identifié, sans la transformer en fait.

### Ce qui reste inconnu

- destructeur ou remise à zéro du receiver ;
- lecteur de `context+0x30` hors de la famille NDXR inspectée ;
- durée de vie de la zone allouée et propriétaire de sa libération ;
- nature des éléments construits dans la zone (`r25`) ;
- rôle métier des valeurs produites par les dispatchs.

Le résultat est donc une meilleure contrainte de durée de vie, pas une preuve
de sémantique gameplay.

## Décision de preuve

`confirmed` :

- le receiver NDXR initialise `+0x30` à zéro ;
- le worker publie une zone à `context+0x30` après avoir utilisé `+0x28` et
  `+0x5c` comme entrées ;
- l'absence de branche directe vers `0x82105bb8` dans les balayages effectués
  ne permet pas de conclure à l'absence d'appel dynamique.

`unknown` :

- type et propriétaire de `context+0x30` ;
- destructeur et consommation indirecte ;
- sémantique applicative.

Aucune session Xenia, interaction graphique ou action humaine n'est requise
pour cette frontière : la prochaine passe statique peut suivre les writers de
`+0x30`, les tables de fonction et les thunks avant toute instrumentation.

## Validation documentaire

- `DumpRange.java` sur `0x82105a80..0x82106400` : PASS ;
- `DumpRange.java` sur `0x820f9b00..0x820faf40` : PASS ;
- `InspectFunctionIsland.java` sur `0x820fa240..0x820fa380` : PASS ;
- `FindPpcBranchesTo.java` pour `0x82105bb8` : PASS, aucun branchement direct
  trouvé ;
- build AC6 : PASS ;
- CTest AC6 : **41/41 PASS** ;
- `git diff --check` : PASS ;
- aucun projet Ghidra, XEX, sortie générée ou runtime modifié : PASS.
