# AC6 — contrat statique des mots de table NDXR et des records (cycle 150)

## Cible et méthode

- target : `ac6-xbox360-pal-default-xex` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet de référence : `ghidra-projects/ace-combat-6` ;
- mode : `analyzeHeadless -readOnly -noanalysis`, avec `DumpRange.java` ;
- aucune modification du XEX, du projet Ghidra, des exports ou du runtime.

Cette passe suit les consommateurs du tableau `owner+0x28` et les écritures
associées à `owner+0x5c`. Elle cherche à qualifier les champs d’un mot de
table avant le dispatch virtuel du worker, sans attribuer de sémantique de
renderer ou de volume à partir de l’offset seul.

## Résultats statiques

### Corps constructeur-associé `0x820fa9c0`

Le corps contigu associé à l’initialisation du sous-objet initialise les champs
`+0x28` et `+0x5c` par des lookups distincts :

```text
0x820fac34  stw r11,0x28(r31)   ; lookup index 0xb via 0x82234e08
0x820fac48  stw r11,0x5c(r31)   ; lookup index 0   via 0x82234e08
```

Une branche de type `4` efface ensuite ces deux champs :

```text
0x820fad0c  lwz r10,0x5c(r31)
0x820fad10  cmplwi r10,4
0x820fad18  stw r26,0x28(r31)
0x820fad1c  stw r26,0x5c(r31)
```

Lorsque `+0x28` est non nul, le même corps :

- lit `+0x74` comme limite de traitement ;
- initialise des compteurs `ushort` dans une zone à partir de `+0x71b4`, avec
  un stride de `0xec` par record ;
- dérive une base de travail de `+0x5c - 0x1000` et réserve une zone en
  `+0x71ac` ;
- parcourt les mots de la table située à `+0x28` et met à jour les records
  indexés.

Dans ces boucles, les mêmes sous-champs sont décodés de façon répétée :

```text
field_hi9 = (word >> 16) & 0x1ff   ; rlwinm ...,0x10,0x17,0x1f
field_lo16 = word & 0xffff         ; rlwinm ...,0x0,0x10,0x1f
field_kind3 = (word >> 27) & 0x7   ; rlwinm ...,0x5,0x1d,0x1f
```

Le champ `field_hi9` est utilisé comme index de record :

```text
record = owner + field_hi9 * 0xec
```

Le record fournit notamment `+0x71b0`; les compteurs et tableaux associés sont
adressés avec le même stride `0xec`. `field_lo16` et `field_kind3` alimentent
les offsets et la sélection de sous-table. Cette preuve confirme la présence
d’un mot de descripteur structuré et d’un index borné de record, pas la nature
du pointeur `+0x28` ni le rôle fonctionnel du record.

### Worker `0x82105bb8` et chemins répétés

Le worker conserve `r18 = owner`, vérifie `owner+0x28 != 0` et le seuil de
`owner+0x5c`, puis lit les mêmes mots depuis `owner+0x28` :

```text
0x82105c88  lwz r31,0(r28)
0x82105c8c  rlwinm r4,r31,0x10,0x17,0x1f
0x82105c98  lwz r10,0x5c(r10)
0x82105ca8  bctrl
0x82105cac  or r5,r3,r3
0x82105cb0  rlwinm r6,r31,0x0,0x10,0x1f
```

Les blocs équivalents commencent à `0x82105f78` et `0x82106180`. Le worker
transmet donc le champ haut de 9 bits au dispatch virtuel comme `r4`, puis le
résultat du dispatch comme `r5` et le champ bas de 16 bits comme `r6`. Le corps
constructeur-associé montre indépendamment que le même champ haut sert d’index
de record à stride `0xec`.

## Ce que cette corrélation permet de conserver

- `owner+0x28` est une table ou base de descripteurs consommée par plusieurs
  boucles ; son contenu n’est pas un simple pointeur opaque sans structure.
- Les mots de cette table possèdent au moins les sous-champs `hi9`, `lo16` et
  `kind3`, confirmés par des extractions répétées dans deux régions de code.
- `hi9` indexe une famille de records à stride `0xec` dans le chemin de
  préparation ; il ne doit pas être nommé adresse, pointeur ou handle sans
  preuve supplémentaire.
- `owner+0x5c` participe à la préparation de cette famille et au choix du slot
  virtuel, mais son contenu mémoire ne doit toujours pas être confondu avec
  `vtable+0x5c`.
- Le worker et le corps de préparation partagent une convention de découpage du
  mot, sans que cela suffise à identifier la cible effective du dispatch.

## Contradiction ABI non résolue

La contradiction reste explicite :

```text
worker : r4 = (word >> 16) & 0x1ff
feuille candidate 0x82101be0 : lhz r11,0x1c(r4)
```

Les preuves statiques ne permettent pas encore de décider si le callee effectif
diffère de la feuille candidate, si `r4` est transformé par un wrapper, ou si
la décompilation de la feuille est appliquée hors de son contexte réel. Il est
donc interdit de qualifier `r4` de pointeur de record ou de publier une ABI
native à partir de cette seule corrélation.

## Statuts de confiance

- découpage `hi9/lo16/kind3` des mots de table : `confirmed` statique ;
- stride de record `0xec` et utilisation de `hi9` comme index dans le corps de
  préparation : `confirmed` statique ;
- association fonctionnelle des records avec NDXR, draw ou volume : `unknown`;
- vtable effective du dispatch et callee de `bctrl` : `unknown`/`cross-match` ;
- interprétation de `r4` dans la feuille `0x82101be0` :
  `needs-dynamic-evidence`.

Aucune action humaine, aucun run Xenia et aucune capture manuelle ne sont
nécessaires pour cette passe. Une capture dynamique ne deviendra justifiée que
si les recherches statiques des writers indirects et du callee effectif ne
réduisent plus la contradiction.

## Validation documentaire

- `DumpRange.java` sur `0x820fa9c0` et le worker : PASS ;
- corrélation des extractions et strides par lecture seule : PASS ;
- aucun fichier généré, binaire ou projet Ghidra modifié : PASS.
