# Cycle 1126 — le placement initial : non trouvé, et une confusion à corriger

Date : 2026-08-08. Ce cycle ne répond pas à sa question. Il élimine, il corrige,
et il dit où il s'arrête.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## La correction, d'abord

Les cycles 1124 et 1125 parlaient d'« objets » sans distinguer deux choses que
`0x820A7070` construit séparément. Le voici en clair :

```
820a7638  bctrl                 ; gestionnaire->virtuelle +0x10 -> l'UNITÉ
820a7648  stw r21,0xd0(r16)     ; son index d'enregistrement
820a764c  stw r14,0xd4(r16)     ; ses drapeaux de camp
820a7650  bl 0x8226fec0         ; insertion dans CX360UnitManager
...
820a76c4  lwz r25,0x8(r26)      ; mot 2 de l'enregistrement = la liste Obj
820a7774  ...                   ; puis une ENTITÉ Obj par enregistrement ObjBin
```

**230 unités** d'un côté, **434 entités `Obj`** de l'autre. La transformation
identité à translation nulle que le cycle 1124 a lue est celle des **entités
`Obj`**, pas des unités.

Et c'est l'**unité** que le programme d'ordres pilote : à `0x82295C0C` l'ordre
d'étiquette 2 lit `+0xE4`, champ que seule l'unité porte (`0x820A7C88`). Donc
l'unité a elle aussi une transformation, et `unité+0x50` est sa position.

Le commentaire de `retail_mission_state.h` porte désormais cette distinction.

## Ce que `0x820A7070` écrit dans une unité

Toutes les écritures vers `r16`, sur la fonction entière :

```
+0x60  +0xD0  +0xD4  +0xD8  +0xDC  +0xE0  +0xE4
```

**Jamais une position.** Le chemin de chargement ne place donc pas les unités
non plus — le constat du cycle 1124 vaut pour les deux familles, cette fois pour
la bonne raison.

## Ce qui a été éliminé

| piste | résultat |
| --- | --- |
| les trois autres sous-genres de l'ordre d'étiquette 2 (`0x822961CC`, `0x82296260`, `0x822962BC`) | tous lisent `unité+0x50` comme position **courante** et calculent des directions ; **aucun ne l'écrit** |
| `0x8226B618`, la préparation de début de mission appelée par l'entrée d'état `0x82258D88` | remet à zéro la table de factions (`contexte+0x58`, foulée `0x44`) et la table de compteurs (`contexte+0x5C`, foulée `0x14`) ; **ne place rien** |
| le bloc `Maneuver` que l'entité `Obj` classe en `+0x210` | **aucun flottant à l'échelle du monde** dans aucun de ces blocs |
| la charge utile elle-même | 19 743 mots sont des flottants à l'échelle du monde, mais hors des 890 enregistrements d'étiquette 2 ce sont des valeurs rondes — 10 000, 30 720, 50 000, 25 000 — des portées et des altitudes, pas des triplets |

## Ce qui a échoué, et pourquoi le dire

Trois balayages, tous trop larges pour désigner qui que ce soit :

| prise | sites | pourquoi elle ne coupe pas |
| --- | ---: | --- |
| écritures vers `+0x50` | 797 | le déplacement est partagé par tout le binaire |
| fonctions touchant `+0x50` **et** `+0xA0` (la copie de trame précédente) | 640 | idem, et `r1+0x50` de pile s'y mêle |
| lecteurs de `objet+0x180` | 370 | trop de classes ont un champ à ce déplacement |

Un déplacement de structure n'est pas une prise quand la structure n'est pas
identifiée. La prise suivante est nommable et ce cycle ne l'a pas prise : **la
classe de l'unité**. Elle sort de la virtuelle `+0x10` du `CX360UnitManager` ;
sa vtable donnerait ses méthodes, et un « poser » s'y trouverait ou n'y serait
pas. C'est là que le prochain cycle commence.

## Ce que cela n'établit pas

- **Le placement initial**, qui reste la question ouverte. Ce cycle ne
  l'approche que par élimination.
- Il reste possible qu'il n'y ait **pas** de placement au sens d'une écriture
  unique — que la position vienne d'une intégration dès la première trame — mais
  rien ici ne le montre, et l'affirmer serait la règle plausible sans contrôle
  que ce projet refuse depuis les cycles 1111 et 1113.
