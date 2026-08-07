# Cycle 1111 — la carte des états

Date : 2026-08-09. Le cluster délimité au cycle 1110, lu comme ce qu'il est.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Plage `0x822E0000..0x822F0000`, **140 fonctions délimitées**.
- **Statique seul.** Artefact : `analysis/state-machine/mission-state-map.tsv`.

## Ce qu'est un état, mécaniquement

Un état n'est jamais appelé : il est **nommé**. Un gestionnaire matérialise
l'adresse d'un autre gestionnaire dans un pointeur de membre — deux mots, le
code et l'ajustement — et le confie au moteur du cycle 1109. Le graphe de
références donne donc les états sans qu'on ait à les deviner :

```
AC6_GRAPH_FUNCTIONS 140
états (fonctions nommées comme donnée) : 33
gestionnaires (testent un signal négatif) : 32
appelants du moteur : 34
```

L'ajustement vaut `0x348` dans chaque pointeur — l'offset de la machine dans
l'objet, ce qui recoupe le cycle 1109.

## Une règle proposée, puis rejetée

Première tentative pour distinguer le super-état des cibles de transition : « le
super-état est le dernier état nommé dans le corps ». Elle est **fausse**, et le
test qui la tue est net : la relation obtenue contient **dix cycles**, alors
qu'une hiérarchie ne peut pas en avoir.

La raison se lit dans le code : le bloc de repli n'est pas à la fin de la
fonction. Il vient **juste après la chaîne de comparaisons de signaux**, avant
les branches d'entrée et de sortie qui, elles, occupent les adresses hautes.

## La règle qui tient

Le super-état est l'adresse matérialisée sur le **chemin de repli**, celui
qu'on atteint quand aucune comparaison de signal n'a mordu :

```
822ed720  cmpwi cr6,r5,-0x3     ; entrée
822ed72c  cmpwi cr6,r5,-0x2
822ed734  cmpwi cr6,r5,-0x1     ; sortie
822ed73c  lis   r10,-0x7dd2     ; aucun signal reconnu : le repli
822ed744  li    r9,0x348        ; l'ajustement du pointeur de membre
822ed74c  addi  r10,r10,0x39a8  ; 0x822E39A8, le super-état
822ed758  stw   r10,0x60(r1)
```

Appliquée aux 33 états, elle résout **10 arêtes** et la relation est
**acyclique** — le contrôle que la première règle échouait.

## La hiérarchie récupérée

```
0x822E39A8  (racine)
├── 0x822E5280   code 10
├── 0x822E8660   code 3
│   └── 0x822E73B0   code 11
├── 0x822ED708   code 1
│   ├── 0x822EACC0   code 7
│   ├── 0x822EB090   code 6
│   └── 0x822ED070   code 5
├── 0x822EDC58
└── 0x822EE5C0

0x822E7E68
└── 0x822E8A90
```

**La racine n'est ni une fonction ni une entrée `.pdata`** — `0x822E39A8` est du
code que le linker n'a pas enregistré. Son corps ne sauvegarde aucun registre et
construit un pointeur de membre nul (`li r10,0x0` … `li r10,-0x1`) : c'est
exactement la forme d'un état sommet, qui répond « je n'ai pas de parent ».

## Les codes d'état

Chaque état publie un petit entier dans `this+0x260` sur sa branche d'entrée —
le champ que `mission_manager_update` teste depuis le cycle 1093. Dix-huit
états en publient un :

| état | code | état | code |
| --- | ---: | --- | ---: |
| `0x822EB5A8` | 2 | `0x822E7598` | 9 |
| `0x822E8660` | 3 | `0x822E5280` | 10 |
| `0x822E4A48` | 4 | `0x822E73B0` | 11 |
| `0x822ED070` | 5 | `0x822E70B0` | 12 |
| `0x822EB090` | 6 | `0x822E35F8` | 14 |
| `0x822EACC0` | 7 | `0x822E3640` | 14 |
| **`0x822E7760`** | **8** | `0x822E3560` | 1 |

Et le code **8** est celui que le cycle 1093 avait relevé sans savoir d'où il
venait : `mission_manager_update` ne parcourt le `CX360ObjManager` de
`contexte+0x2E8` que sous la garde `état+0x260 == 8`.

**`0x822E7760` est donc l'état pendant lequel la mission tourne.** C'est aussi
l'un des états que le cycle 1107 a croisés : sa branche d'entrée est atteinte
par `0x822E79B0`, la fonction que le bit « aucun compte à rebours en cours »
garde.

## Ce que cela n'établit pas

- **La hiérarchie est partielle** : 10 arêtes de parenté sur 33 états. Les 23
  autres n'ont pas résolu le motif — dispatch par table de saut, repli hors
  fenêtre, ou forme que ce script ne reconnaît pas. Aucune n'est déclarée
  absente : elle est **non résolue**.
- **Les codes ne sont pas attribués par branche.** Le script relève tous les
  `stw` vers `+0x260` sans savoir dans quelle branche ils sont ; la valeur `1`
  revient chez huit états et n'est donc probablement pas leur code d'entrée.
  Les deux attributions vérifiées par lecture directe sont `0x822E8660 → 3` et
  `0x822EACC0 → 7`.
- **Aucun état n'est nommé.** On a des numéros et une arborescence, pas des
  intitulés. Dire que le code 8 est « en vol » serait une inférence de genre ;
  ce que le binaire dit, c'est que le gestionnaire de mission n'agit que là.
- Les transitions proprement dites — quel état va vers quel autre — ne sont pas
  séparées des arêtes de parenté dans le graphe brut de 33 nœuds.
