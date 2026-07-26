# AC6 — vtables et nettoyage du receiver NDXR (cycle 159)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

La passe est headless et en lecture seule. Elle combine des recherches de mots
PPC exacts avec `FindU32`, un balayage de branches avec `FindPpcBranchesTo` et
des dumps bornés autour de `0x820fa258..0x820fa9c0`. Aucun projet Ghidra n'est
écrit.

## Résultats

### Transitions de vtable

Les encodages exacts de `subi r11,r11,0x365c` apparaissent à :

```text
0x820fa278
0x820fa704
```

Dans les deux cas, la valeur construite est `0x8205c9a4`, l'address-point
NDXR déjà qualifié. L'encodage de `subi r11,r11,0x37d4`, qui construit
`0x8205c82c`, apparaît à `0x820f9d98` et `0x820fa798`.

Le chemin autour de `0x820fa6f0` fait donc :

```text
0x820fa704  stw  0x8205c9a4,0(r31)
0x820fa70c  bl   0x820fa7a8
...
0x820fa798  stw  0x8205c82c,0(r31)
```

La séquence est compatible avec une routine de destruction/nettoyage qui
entre avec le vtable dérivé, appelle son corps de nettoyage, puis installe le
vtable de base. Le nom C++ exact reste volontairement non confirmé.

### Wrapper de destruction

Le chemin à `0x820fa598` conserve `r4`, appelle `0x820fa6f0` à `0x820fa5b4`,
extrait ensuite son bit 31 avec `rlwinm`, et appelle `0x82380070` lorsque ce
bit le demande. C'est le motif statique d'un wrapper de destruction avec
libération conditionnelle ; il ne prouve pas à lui seul l'allocateur exact.

Un second branchement vers `0x820fa6f0` est trouvé à `0x823d4ed8` et reste
non qualifié.

### Nettoyage des champs

Le corps autour de `0x820fa7a8` remet notamment à zéro :

```text
receiver+0x0c .. receiver+0x3c
receiver+0x5c
receiver+0x70
receiver+0x74
receiver+0x7c
receiver+0x4088 / +0x4090
receiver+0x44b0 .. +0x44cc
```

Le champ `receiver+0x30`, publié par le worker à `0x82106344`, est donc
explicitement effacé par ce chemin de nettoyage. Le dump ne montre pas de
lecture de l'ancien contenu de `+0x30` avant cet effacement, ni d'appel de
libération recevant directement l'ancien pointeur `+0x30`. La propriété de la
zone publiée et son éventuelle libération restent `unknown`.

## Décision de preuve

`confirmed` :

- `0x8205c9a4` est réinstallé au début du chemin de nettoyage puis
  `0x8205c82c` à sa sortie ;
- le wrapper autour de `0x820fa598` teste un indicateur de destruction après
  l'appel et peut appeler `0x82380070` ;
- le corps de nettoyage remet à zéro `+0x30`, `+0x5c` et `+0x74` ;
- le receiver NDXR et la zone publiée du worker ont une frontière de durée de
  vie distincte des flottants homonymes de `0x820f9dc8`.

`unknown` :

- nom et hiérarchie C++ exacts ;
- allocateur réel utilisé pour `context+0x30` ;
- consommateur indirect de la zone ;
- raison pour laquelle le nettoyage ne libère pas directement l'ancien
  contenu visible dans cette île de code.

Cette frontière ne demande pas d'action humaine. La prochaine passe statique
doit suivre les appels aux helpers de libération (`0x82335f38`, `0x82109320`,
`0x821065e0`) et le branchement externe `0x823d4ed8`, sans renommer le champ en
`buffer`, `workspace` ou équivalent métier avant preuve supplémentaire.

## Validation documentaire

- `FindU32.java 0x396bc9a4` : PASS (`0x820fa278`, `0x820fa704`) ;
- `FindU32.java 0x396bc82c` : PASS (`0x820f9d98`, `0x820fa798`) ;
- `FindPpcBranchesTo.java` vers `0x820fa6f0` : PASS ;
- `DumpRange.java` sur `0x820fa4e0..0x820fa6f4` : PASS ;
- `DumpRange.java` sur `0x820fa840..0x820fac20` : PASS ;
- aucune écriture Ghidra/XEX/générée/runtime : PASS.
