# Cycle 1124 — le consommateur du sous-record `Obj` ne lit pas ses flottants

Date : 2026-08-08. La question laissée ouverte par le cycle 1122.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon` (VMX128), **hors du projet
  canonique**, avec quatre arms de table de sauts désassemblés dans ce corpus de
  travail seul : `0x820A7744`, `0x820A7750`, `0x820A775C`, `0x820A7770`, plus
  `0x8228F678`. Rien n'est fusionné avec `ghidra-projects/ace-combat-6`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## La question

`build_retail_world` place chaque unité au premier triplet `Obj` de son
enregistrement. Le cycle 1122 avait montré que ce triplet n'est pas une
coordonnée monde — les coordonnées monde sont dans les ordres d'étiquette 2 — et
laissait la vraie question ouverte : **qui lit ces flottants ?**

Réponse : sur le chemin du chargement, **personne**.

## Ce que `0x820A7070` lit du bloc de données `ObjBin`

Le consommateur parcourt les enregistrements `ObjBin` de `0x20` octets remplis
par `ObjBin::read` `0x82330158` (cycle 1096) et lit `enregistrement[0]`, son
pointeur de données. De ce bloc, sur toute la fonction, **trois octets** :

| déplacement | usage |
| --- | --- |
| `+0x56` | la clé de la fabrique `0x820A8138` — table de 15 triplets |
| `+0x61` | un indice d'emplacement d'unité, résolu par `0x8228E9B8` |
| `+0x62` | le second, `0xFF` valant « absent » |

Et rien d'autre. Aucun `lfs`, aucun `lvlx` sur ce bloc.

Mesuré sur la charge utile, par le parseur natif : **434 enregistrements
`ObjBin`** — le compte du cycle 1096, retrouvé par une autre voie — et les trois
octets valent **0 partout**.

## Ce que l'objet construit reçoit à la place

L'objet sort de la fabrique, puis :

```
820a77a0  addi r11,r31,0x10      ; le biais de +0x10 qu'on retrouvera plus bas
820a77bc  stfs f31,0x40(r11)     \
820a77c0  stfs f31,0x44(r11)      >  objet+0x50, +0x54, +0x58 = 0
820a77c8  stfs f31,0x48(r11)     /
820a77d0  lvx128 vr0,r0,r5       ; r5 = 0x8204F7F0 = (1,0,0,0)
820a77d8  stvx128 vr0,r0,r9      ; objet+0x20
820a77e0  lvx128 vr0,r0,r5       ; r5 = 0x8204F800 = (0,1,0,0)
820a77e8  stvx128 vr0,r9,r4      ; objet+0x30
820a77ec  lvx128 vr0,r0,r5       ; r5 = 0x8204F810 = (0,0,1,0)
820a77f4  stvx128 vr0,r9,r5      ; objet+0x40
```

C'est **une transformation à quatre lignes — X, Y, Z, translation — initialisée
à l'identité, translation nulle.** Puis les quatre lignes sont recopiées en
`+0x70..+0xA0`, la copie de trame précédente.

Le chemin est **le seul** : la table de sauts de `0x820A772C` a quatre arms, et
les quatre ne font que choisir le genre de fabrique dans `r30` avant de retomber
sur `0x820A7774`.

```
820a7744  cntlzw r11,r24 ; rlwinm r30,... ; b 0x820a7774
820a7750  cntlzw r11,r24 ; rlwinm r30,... ; b 0x820a7774
820a775c  subfic ...     ; addi r30,r11,0x3 ; b 0x820a7774
820a7770  li r30,0x4     ;                    (chute)
```

**Les 434 objets naissent donc à l'origine, sans exception.**

## Et l'enregistrement, lui, est classé

Le consommateur appelle l'emplacement virtuel `+0x50` de l'objet avec
l'enregistrement `ObjBin` en second argument (`0x820A7A40`). Pour la classe que
la clé 0 construit — constructeur `0x8228F6B0`, vtable `0x82008F58` — cet
emplacement est `0x8228F678`, et il tient en quatre écritures :

```
8228f678  stw r4,0x180(r3)   ; objet+0x180 = l'enregistrement ObjBin
8228f67c  lwz r11,0x8(r4)    ; enregistrement+0x08 = le bloc Maneuver
8228f680  stw r11,0x210(r3)  ; objet+0x210
8228f684  lwz r11,0x4(r4)    ; enregistrement+0x04 = le bloc Param
8228f69c  lwz r11,0x0(r11)   ; ... deux champs recopiés
```

Aucune coordonnée. **L'objet garde son enregistrement en `+0x180`** ; qui s'en
sert ensuite pour le déplacer n'est pas établi ici.

## Le bénéfice de côté : les déplacements du cycle 1122 sont ancrés

`0x82270380` rend **`objet+0x10`**, pas l'objet :

```
82270434  addi r3,r31,0x10
```

Donc les `+0x30` et `+0x38` que `0x822953F0` lit pour le cap sont les `+0x40` et
`+0x48` de l'objet — les composantes x et z de la ligne « avant », ce qu'un cap
`atan2(x, z)` exige — et ses `+0x40`, `+0x44`, `+0x48` sont les `+0x50`, `+0x54`,
`+0x58` de l'objet : **la translation**. Le cycle 1122 lisait juste ; il ne
savait pas encore contre quelle disposition.

## Un contre-exemple concret à la carte de classes

La vtable `0x82008F58` de la classe construite ici **n'est pas dans
`analysis/class-map.tsv`**, et le mot qui la précède est `bf490fdb`, c'est-à-dire
le flottant `-0,785398` — pas un localisateur RTTI.

Le cycle 1123 énonçait cette limite en l'air : « une classe sans localisateur
complet resterait invisible ». En voici une, et ce n'est pas une classe
marginale : c'est celle que les 434 objets de la Mission 01 instancient.

## Effet sur le produit natif

Aucun changement de comportement, et c'est délibéré. Reproduire fidèlement ce
qui vient d'être lu voudrait dire **placer les 230 unités à l'origine**, ce qui
serait fidèle à une lecture partielle et rendrait le monde inutile. Le commentaire
de `retail_mission_state.h` dit désormais la chose exacte : ce champ n'est pas
« un décalage là où il faudrait une coordonnée monde », c'est **un nombre que
retail ne consulte pas sur ce chemin**, rempli parce que le runtime en veut un.

`ctest 24/24`. La porte JF reste verte.

## Ce que cela n'établit pas

- **Qui déplace les objets.** L'enregistrement est en `objet+0x180` ; le
  déplacement est ailleurs, et un balayage des 370 accès à `+0x180` du corpus n'a
  pas suffi à le désigner — l'offset est partagé par trop de classes. C'est le
  travail suivant, et il commence là.
- **Ce que sont `+0x61` et `+0x62`.** Deux indices d'emplacement passés à
  `0x8228E9B8` ; ils valent 0 partout sur cette charge utile, donc rien n'y est
  exercé.
- **Les quatorze autres clés** de la fabrique `0x820A8138`. La Mission 01
  n'exerce que la clé 0.
