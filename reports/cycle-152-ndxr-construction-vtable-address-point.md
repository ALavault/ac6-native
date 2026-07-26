# AC6 — address-point de vtable et résolution statique du dispatch NDXR (cycle 152)

## Cible et méthode

- target : `ac6-xbox360-pal-default-xex` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet : `ghidra-projects/ace-combat-6` ;
- mode : `analyzeHeadless -readOnly -noanalysis` ;
- scripts : `DumpRange.java`, `DumpDataWords.java`, `ReferencesTo.java` et
  `FindDirectCallsTo.java` ;
- aucune écriture dans le XEX, le projet Ghidra, les exports ou le runtime.

Cette passe vérifie l'adresse-point réellement pertinente avant le slot virtuel
`+0x5c`. Elle distingue les différents address-points présents dans une table
de vtable et ne déduit pas l'état runtime d'une instance à partir d'un seul
mot statique.

## Résultats

### Le worker reçoit bien l'owner comme receiver

Le corps du worker commence à `0x82105ba8`. Le chemin appelant à
`0x820fbbd4` prépare explicitement le receiver :

```text
0x820fbbd0  or   r3,r31,r31
0x820fbbd4  bl   0x82105ba8
```

Le worker conserve ensuite `r18 = r3`, lit `owner+0x28` et `owner+0x5c`, puis
effectue le dispatch :

```text
0x82105c78  lwz    r11,0x28(r18)
0x82105c7c  or     r3,r18,r18
0x82105c80  lwz    r10,0x0(r18)
0x82105c8c  rlwinm r4,r31,0x10,0x17,0x1f
0x82105c98  lwz    r10,0x5c(r10)
0x82105ca4  mtspr  CTR,r10
0x82105ca8  bctrl
```

Les blocs répétés à `0x82105f78` et `0x82106180` ont la même forme. Le champ
`r4` reste donc un index numérique de 9 bits dérivé du mot de table, et non un
pointeur fabriqué par le worker.

### L'address-point du receiver n'est pas `0x8205c980`

Le balayage des données montre que la table située autour de `0x8205c980`
contient plusieurs address-points. La valeur `0x8205c9a4` est notamment
confirmée comme emplacement contenant le destructeur `0x820fa598` :

```text
0x8205c9a4 -> 0x820fa598
```

Le corps associé `0x820fa6f0` écrit ensuite cette valeur dans le premier mot
de son objet avant d'appeler `0x820fa7a8` :

```text
0x820fa704  subi r11,r11,0x365c   ; r11 = 0x8205c9a4
0x820fa708  stw  r11,0x0(r31)
0x820fa70c  bl   0x820fa7a8
```

`0x820fa7a8` est une étape de destruction/base distincte et installe ensuite
`0x8205c82c`. Il ne faut donc pas prendre cette dernière valeur pour
l'address-point de la classe dont les méthodes alimentent le worker.

Le même address-point est relié à la méthode `0x820fa9c0` par l'entrée :

```text
0x8205ca90 -> 0x820fa9c0
```

Or `0x8205ca90 - 0x8205c9a4 = 0xec`. Le corps `0x820fa9c0` qui prépare
`owner+0x28`, `owner+0x5c`, `owner+0x74` et les records est donc dans la même
famille de vtable que le destructeur et le worker, à l'offset virtuel `+0xec`.

`0x8205c980` reste un autre address-point/une autre vue statique de la table
(notamment utilisé par `0x820f9dc8`). Il ne doit pas être utilisé pour calculer
le slot du worker sans qualifier le sous-objet concerné.

### Le slot worker est `0x82100600`

À partir de l'address-point `0x8205c9a4` :

```text
0x8205c9a4 + 0x5c = 0x8205ca00
0x8205ca00         -> 0x82100600
```

Le corps de la feuille située à `0x82100600` est statiquement présent même si
Ghidra ne lui attribue pas une frontière de fonction autonome :

```text
0x82100600  lwz    r11,0x74(r3)
0x82100604  cmplw  cr6,r4,r11
0x82100608  bge    cr6,0x8210061c
0x8210060c  addi   r11,r4,0x1b63
0x82100610  rlwinm r11,r11,0x2,0x0,0x1d
0x82100614  lwzx   r3,r11,r3
0x82100618  blr
0x8210061c  li     r3,0x0
0x82100620  blr
```

Cette feuille accepte exactement le contrat produit par le worker : `r3`
reste l'owner, `r4` est comparé à la limite `owner+0x74`, puis sert d'index
dans une table relative à l'owner. Elle ne déréférence donc pas `r4+0x1c`.

La feuille `0x82101be0` est une autre entrée statique :

```text
0x8205c9dc -> 0x82101be0
```

Elle se trouve à `+0x38` de l'address-point `0x8205c9a4`, pas à `+0x5c`, et
son contrat local (`lhz 0x1c(r4)`) ne doit plus être utilisé comme cible du
dispatch worker.

## Décision et portée

La contradiction ABI précédente est résolue statiquement au niveau de la
famille de vtable :

```text
worker r4 = (word >> 16) & 0x1ff
address-point 0x8205c9a4
address-point + 0x5c -> 0x82100600
0x82100600 : r4 = index borné dans owner
```

Cela justifie les règles suivantes pour l'adaptateur natif :

- ne pas router le worker vers `0x82101be0` ;
- ne pas transformer `hi9` en pointeur de record ;
- conserver `owner+0x74` comme limite de l'index jusqu'à preuve contraire ;
- garder `owner+0x28` (mot de table) distinct du slot `vtable+0x5c` ;
- qualifier l'association de l'address-point comme `confirmed` statique, mais
  laisser l'état exact de chaque instance runtime à `cross-match` tant qu'une
  trace d'exécution n'a pas observé son premier mot.

Aucune action humaine, aucun run Xenia et aucune capture manuelle ne sont
nécessaires pour cette passe. Une observation dynamique ne sera utile que pour
valider l'état d'une instance particulière après l'intégration du contrat,
pas pour choisir entre les deux feuilles statiques.

## Validation documentaire

- `DumpRange.java` sur le worker, les destructeurs et `0x82100600` : PASS ;
- `DumpDataWords.java` sur `0x8205c82c` et `0x8205c980` : PASS ;
- `ReferencesTo.java` sur `0x820fa598`, `0x820fa9c0` et les address-points :
  PASS ;
- `FindDirectCallsTo.java` : aucun appel direct concurrent ne remplace le
  dispatch `bctrl` ;
- aucun fichier généré, binaire ou projet Ghidra modifié : PASS.
