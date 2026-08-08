# Cycle 1127 — la vtable de l'unité, lue : pas de placeur, mais la route

Date : 2026-08-08. La prise que le cycle 1126 nommait sans la prendre.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## La chaîne jusqu'à la classe

Le chargeur passe le gestionnaire d'unités en premier argument de
`0x820A7070` :

```
8219c748  addis r22,r28,0x13 ; 8219c760 subi r22,r22,0x4bc0   -> contexte+0x12B440
```

que le cycle 1096 nomme déjà **`CX360UnitManager`**, vtable `0x82055190`. Son
emplacement `+0x10` — celui que `0x820A7638` appelle pour construire l'unité —
est **`0x820A7F48`**, la fabrique que le contrat cite depuis J1. Elle alloue par
catégorie :

| catégorie | constructeur | taille | vtable |
| ---: | --- | ---: | --- |
| 1 | `0x822A6560` | `0x100` | `0x820568D4` |
| 2 | `0x822A6560` puis remplacement | `0x100` | `0x82056934` |
| 3 | `0x822A8570` | `0x230` | `0x82009AB0` |
| 4–6 | `0x820A8E08` | — | — |

Toutes les factions de la Mission 01 portent le code de camp 0, donc catégorie 1
— ou 2 pour le joueur local. La classe est nommée par la carte du cycle 1115 :

```
0x820568d4  ACE6::CAce6UnitPlayer   bases: CAce6UnitPlayer@0 ; CAce6Unit@0
```

Sa taille `0x100` confirme au passage la séparation du cycle 1126 : `+0x180`,
où l'entité `Obj` classe son enregistrement, **n'existe pas** dans une unité.

## Ce que la vtable contient — et ne contient pas

120 emplacements lus. **Aucun n'écrit une position.**

Ce n'est pas une impression : le balayage du cycle 1126 comptait
`addi rX,r1,0x50` — un cadre de pile — comme une écriture de position. En
excluant `r1`, les écrivains de position de **tout** le binaire tombent de 797 à
**52**, et aucun des 120 emplacements n'en fait partie. La première version de ce
cycle avait « trouvé » `0x822A2C80` en `+0x44` ; c'était ce faux positif, et son
`stfs f3,0x50(r1)` est de la pile.

**Le placeur n'est donc pas une méthode virtuelle de `CAce6UnitPlayer`.** C'est
un résultat négatif, et il clôt la prise que le cycle 1126 proposait.

## Ce que la vtable contient à la place : la route

Trois emplacements forment une machinerie cohérente, et elle explique enfin à
quoi sert la liste `Obj` :

**`+0x34` → `0x822A2B08`, « choisir l'entrée N »**

```
822a2b18  lwz r9,0xe0(r3)     ; unité+0xE0 = la liste Obj
822a2b1c  rlwinm r10,r4,0x3   ; N * 8 - les éléments de 8 octets du cycle 1096
822a2b24  lwz r11,0x4(r9)
822a2b2c  lwz r11,0x4(r11)    ; -> le nœud ObjBin
822a2b30  lwz r11,0x4(r11)    ; -> son bloc Param
822a2b34  lwz r4,0x0(r11)     ; -> les données du bloc
822a2b38  bl 0x822a23d8
```

**`+0x44` et `+0xA4` → `0x822A2C80`, « chercher l'entrée qui correspond »** :
parcourt la liste, retient l'entrée dont le bloc Param a `+0x28 == 0`,
`+0x29 == l'argument`, son premier flottant nul et deux bits de `+0x2B` clairs,
puis appelle l'emplacement `+0x34` avec son index.

**Le puits, `0x822A23D8(unité, enregistrement, f1)`** répartit sur l'octet
`+0x2A` de l'enregistrement — `< 1`, `== 1`, `== 2`, `>= 3` abandonne — et le
cas `== 2` va chercher une unité d'ancrage par les octets `+0x2C` et `+0x2D`
**avec `0x82270380`, sur le même gestionnaire que `0x822953F0`**. C'est la même
forme d'enregistrement de position que le cycle 1122 — triplet, octet de mode,
paire d'ancre — à d'autres déplacements.

Le cas mode 0 :

```
822a27b4  lfs f0,0x4(r30)   \
822a27d0  lfs f0,0x8(r30)    >  le triplet +0x04 +0x08 +0x0C
822a27d8  lfs f0,0xc(r30)   /
822a27c8  li r11,0xc0
822a27e8  stvx128 vr0,r31,r11   ; *(unité + 0xC0) = ce triplet
822a27f0  lfs f1,0x18(r30)
822a27ec  lfs f2,0x1c(r30)
822a27f4  bl 0x822a1e80
```

**`unité+0xC0` est une destination, pas la position** — la position reste
`+0x50`, que rien ici n'écrit.

Trois champs d'unité en sortent, tous établis : `+0xC0` la destination, `+0xE0`
la liste `Obj` — donc **la route** —, `+0xF0` le curseur dans cette liste.

## Le contrôle qui refuse la conclusion facile

Il serait tentant d'écrire « la naissance est le premier point de la route ».
Mesure sur les 434 blocs `Param` de la Mission 01 :

```
octet de mode +0x2A : {0: 434}
mode 0, x : 0 … 60 000   médiane 6 000
mode 0, y : 0 … 0
mode 0, z : 0 … 2,4
```

**y est nul partout et z l'est presque.** Ce n'est pas un triplet de coordonnées
sur cette charge utile, quoi qu'en fasse `0x822A27B0`. La règle plausible est
donc morte avant d'avoir été écrite, comme aux cycles 1111 et 1113.

## Ce que cela n'établit pas

- **Le placement initial**, toujours pas trouvé. Ce qui est acquis, c'est qu'il
  n'est ni sur le chemin de chargement (cycle 1126), ni dans les sous-genres de
  l'ordre d'étiquette 2 (1126), ni dans la vtable de la classe d'unité (ici).
- **Ce que sont les trois flottants du bloc `Param`.** Leur consommateur les
  traite comme un vecteur ; la charge utile ne les remplit pas comme tel. L'un
  des deux se trompe sur cette mission, et rien ici ne dit lequel.
- **Les 52 écrivains de position du binaire** sont maintenant une liste courte et
  aucun n'a été lu. C'est la prise suivante, et elle est enfin de taille humaine.
