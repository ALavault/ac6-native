# AC6 — vtable NDXR et lecteur qualifié de `+0x30` (cycle 162)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

La passe a utilisé Ghidra headless en lecture seule avec `DumpDataWords.java`,
`FindMemoryScalarInRange.java` et `DumpRange.java`. Le address-point de la
vtable NDXR est `0x8205c9a4`; ses 80 entrées ont été exportées avant de
chercher les opérandes mémoire `0x30`, `0x28`, `0x5c` et `0x74`.

## Résultat de la vtable

La vtable contient notamment :

- `+0x00 -> 0x820fa598` ;
- `+0x58 -> 0x821005e8` ;
- `+0x5c -> 0x82100600` ;
- `+0x60 -> 0x82100628` ;
- `+0x108 -> 0x821033a8` ;
- `+0x10c -> 0x820fbc28` ;
- `+0x110 -> 0x820fa9c0` ;
- `+0x114 -> 0x820fa7a8`.

Les autres slots ont aussi été conservés dans la sortie headless de la passe ;
les entrées `+0x58..+0x60` restent les lecteurs/worker déjà qualifiés. Aucun
slot de cette vtable ne pointe directement vers `0x82102148`.

## Lecteur trouvé

Le balayage des instructions a trouvé un lecteur réellement qualifié :

```text
0x82102148  mfspr r12,LR       ; début d'un corps PPC avec prologue
0x82102160  or   r31,r3,r3     ; receiver conservé en r31
0x8210217c  lwz  r8,0x28(r31)
0x8210218c  lwz  r7,0x30(r31)
0x82102198  lwz  r11,0x5c(r31)
0x8210232c  lwz  r10,0x28(r31)
0x82102374  lwz  r8,0x5c(r8)
0x8210244c  lwz  r11,0x74(r31)
0x82102450  lwz  r10,0x78(r31)
0x82102504  lwz  r11,0x0(r31)
0x8210250c  lwz  r11,0x8(r11)
0x82102514  bctrl             ; appel virtuel sur le même receiver
```

Le corps vérifie `+0x28`, `+0x30` et `+0x5c`, borne deux indices dans `0..15`,
calcule une adresse dans la zone pointée par `+0x30`, lit une entrée, puis
utilise `+0x74/+0x78` pour la validation de bornes. Il termine par un appel
virtuel `vtable+0x8` sur `r31` et non par un appel direct au service de
libération `0x82222f20`.

Cette observation confirme qu'un consommateur de la zone publiée par le worker
existe dans le binaire. Elle ne suffit pas encore à lui donner un nom C++ ou à
prouver qu'il s'agit d'un slot de la vtable `0x8205c9a4` : son adresse apparaît
dans le bloc Ghidra `.pdata`, intercalée avec le mot de métadonnées de
déroulement associé :

```text
0x8207c1c8 -> 0x82102148  (entrée de fonction)
0x8207c1cc -> 0x40010706    (métadonnée `.pdata`)
0x8207c1d0 -> 0x82102568  (entrée suivante)
0x8207c1d4 -> 0x40024105    (métadonnée `.pdata`)
```

Le bloc `.pdata` borne donc le corps de `0x82102148` à l'intervalle
`0x82102148..0x82102567`. Il ne fournit ni relation d'appel ni relation avec
l'instance NDXR : cette appartenance reste à établir par suivi du registre, du
constructeur ou d'un appel indirect.

## Faux positifs écartés

- `0x820fb4f4 lfs f10,0x30(r11)` lit un élément de données dans une boucle de
  géométrie, avec une base calculée, pas le receiver NDXR.
- `0x820fb988 lfs f10,0x30(r30)` et `0x820fb9c0 stfs f29,0x30(r29)` appartiennent
  à un sous-objet flottant qui reçoit plusieurs composantes `+0x28..+0x44`.
- `0x820fc008` et `0x820fe08c` ne font qu'utiliser la constante immédiate
  `0x30`.

## Décision de preuve

`confirmed` :

- le champ publié par le worker (`context+0x30`) a au moins un lecteur statique
  qui est qualifié par le même receiver que `+0x28/+0x5c` ;
- le lecteur vérifie aussi `+0x74/+0x78` et indexe la zone pointée par `+0x30` ;
- il ne faut plus déclarer l'absence de lecteurs de `+0x30`.

`confirmed` :

- `0x82102148` est une entrée `.text` enregistrée par `.pdata` à
  `0x8207c1c8`, avec la métadonnée voisine `0x40010706` ;
- l'entrée `.pdata` suivante `0x82102568` fournit une borne statique du corps.

`cross-match` :

- le corps lit les mêmes offsets `+0x28/+0x5c/+0x74` que le receiver NDXR,
  mais son appartenance directe à la vtable `0x8205c9a4` n'est pas démontrée.

`unknown` :

- nom C++ et rôle métier du lecteur ;
- appelant ou dispatcher exact qui invoque le corps (le bloc `.pdata` n'est pas
  une table de dispatch) ;
- propriétaire et libérateur final de la zone ;
- signification des deux indices et des valeurs lues.

La frontière restante est donc une qualification de dispatch et de durée de vie,
pas un blocage nécessitant une intervention humaine ou une session Xenia.

## Validation documentaire

- dump de la vtable `0x8205c9a4`, 80 mots : PASS ;
- balayage `+0x30/+0x28/+0x5c/+0x74` : PASS ;
- export `0x82102148..0x82102570` : PASS ;
- confirmation du bloc `.pdata` et des bornes `0x82102148..0x82102567` : PASS ;
- aucune écriture Ghidra/XEX/sortie générée/runtime : PASS.
