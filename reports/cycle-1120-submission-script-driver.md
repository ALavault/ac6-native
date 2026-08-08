# Cycle 1120 — qui fait avancer le script, et qui met fin à la mission

Date : 2026-08-08. Le comportement `mission_completion`, en statique.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## La question laissée ouverte au cycle 1097

Le cycle 1097 avait lu les deux moitiés du script — `0x8226E908` sélectionne
une sous-mission, `0x8226E158` exécute un pas — et s'était arrêté là, sur un
constat gênant : **la Mission 01 ne contient aucun pas d'étiquette 7**, donc
aucune condition de compteur ne la fait progresser. La conclusion d'alors,
« les sous-missions s'enchaînent par le chemin des étiquettes 1, 4, 5, 6 et 8,
qui délèguent à `0x82267370` selon le mode de jeu », nommait le délégué sans
jamais le lire.

`0x82267370` est le curseur. Le voici, instruction par instruction.

## `0x82267370` — l'avance

```
82267384  lwz r11,0x268(r31)      ; le pas courant
8226738c  bne 0x822673a8
82267390  lwz r11,0x10(r31)       ; l'index de sous-mission
82267398  bne 0x822673a8
8226739c  lwz r11,0x14(r31)       ; l'index de pas
822673a4  beq 0x822673b4          ; les trois nuls : ne pas incrémenter
822673a8  lwz r11,0x14(r31)
822673ac  addi r11,r11,0x1
822673b0  stw r11,0x14(r31)       ; sinon, pas suivant

822673d8  rlwinm r10,r11,0x4,...  ; sous-mission * 0x10
822673e4  lwz r10,0x8(r10)        ; l'entrée + 8
822673e8  lwz r10,0x0(r10)
822673ec  lbz r10,0x0(r10)        ; son octet de compte de pas
822673f0  cmpw cr6,r8,r10
822673f4  blt cr6,0x82267434      ; pas < compte : exécuter ce pas

822673f8  addi r11,r11,0x1
822673fc  stw r11,0x10(r31)       ; sinon : sous-mission suivante
82267400  li r11,0x0
82267404  stw r11,0x14(r31)       ; pas remis à zéro
82267408  bl 0x82093dd0           ; le compte de sous-missions du slot 2
82267410  cmpw cr6,r11,r3
82267414  blt cr6,0x82267430
82267418  li r3,0x1               ; index >= compte : le script est fini
8226742c  blr

82267434  bl 0x8226e158           ; exécuter le pas courant
```

Trois faits en découlent, et aucun n'est une hypothèse :

1. **Le premier appel n'incrémente rien.** La garde des trois lectures n'est
   ouverte qu'à l'entrée, quand le curseur, l'index de pas et le pointeur de pas
   valent tous zéro. C'est ce qui fait exécuter le pas 0 de la sous-mission 0 au
   lieu de le sauter.
2. **La borne des pas est un octet de la charge utile**, la tête du bloc de
   données de la liste de pas — pas le compte déclaré par la table. Les deux
   sont lus séparément par le portage, et ils coïncident sur cette mission.
3. **Le retour 1 est le seul terminateur** : l'index de sous-mission atteint le
   compte du slot 2, lu par `0x82093DD0` sur le même octet que `0x8226E908`.

## Qui l'appelle, et ce qui suit le retour 1

Trois appelants. Deux comptent :

**`0x82258D88`, branche d'entrée (`param_2 == -3`), à `0x8225912C`.** Le coup
d'envoi : le seul appel qui trouve les trois champs à zéro.

**`0x822ED708`, signal `-2`, à `0x822ED800`.** C'est le gestionnaire d'un état
de la machine du cycle 1112 — `0x822ED708`, profondeur 1, code d'entrée 1,
sur-état des six états de profondeur 2. Sa branche de mise à jour :

```
822ed7c8  lwz r11,0x70(r1)        ; contexte+0x820
822ed7d0  beq 0x822edb64          ; nul : rien
822ed7d8  bl 0x82268c58
822ed7e0  beq 0x822edb64          ; pas d'objet : rien
822ed7ec  lwz r11,0x124(r3)
822ed7f0  rlwinm r11,r11,0x0,0x1a,0x1f
822ed7f8  bne 0x822edb64          ; six bits bas posés : rien
822ed800  bl 0x82267370           ; avancer d'un pas
822ed80c  beq 0x822ed834          ; retour 0 : distribuer l'étiquette du pas
822ed814  bl 0x82266710           ; retour non nul : le script est épuisé
```

Le **signal `-2`**, que le cycle 1119 avait nommé « observé et jamais modélisé »,
a donc ici un rôle précis : c'est le signal de mise à jour, et sur l'état de
mission il fait avancer le script.

**`0x82266710`** est appelé de là et de nulle part ailleurs dans le binaire :

```
82266724  li r10,0x1
8226672c  stw r10,0x338(r31)      ; contexte+0x338 = 1
82266730  lwz r11,0xb0(r11) ; bctrl  ; virtuelle +0xB0 du gestionnaire
82266748  bl 0x822cc930
82266754  lwz r11,0xbc(r11) ; bctrl  ; virtuelle +0xBC
```

`contexte+0x338` est lu par 23 fonctions, dont l'état `0x822EB5A8`
(`822eb748`, `822eb87c`) qui refuse son travail de jeu quand il est posé, et
`0x82258B58` qui rend zéro. C'est le drapeau « la mission est finie ». Il prend
aussi la valeur 2 ailleurs (`0x82267868` la teste) ; **ce producteur-là n'est
pas trouvé et n'est pas modélisé**, comme le signal `-2` ne l'était pas.

## Ce que cela permet de prédire, avant de le mesurer

Les comptes de pas de la Mission 01 sont 2, 1, 2, 1 (cycles 1092 et 1097). Le
curseur défini ci-dessus impose donc, à partir de l'entrée :

| appel | curseur avant | effet |
| ---: | --- | --- |
| 1 | (0,0) sans pas courant | exécute (0,0) sans incrémenter |
| 2 | (0,0) | exécute (0,1) |
| 3 | (0,1) | 2 >= 2 → exécute (1,0) |
| 4 | (1,0) | 1 >= 1 → exécute (2,0) |
| 5 | (2,0) | exécute (2,1) |
| 6 | (2,1) | 2 >= 2 → exécute (3,0) |
| 7 | (3,0) | 1 >= 1, puis 4 >= 4 → **retour 1** |

Six pas, chacun une fois, dans cet ordre, puis l'épuisement au septième appel.
C'est une prédiction sur la charge utile, faite avant de l'exécuter, et c'est ce
que le test natif vérifie.

## Ce que cela n'établit pas

- **La cadence.** La garde de `0x822ED708` — `contexte+0x820` non nul et les six
  bits bas de `+0x124` de l'objet de `0x82268C58` nuls — décide *quand* une
  avance a lieu. Aucun des deux champs n'a de contrepartie native ; le portage
  le dit et laisse la cadence à son appelant plutôt que d'inventer une règle.
  Rien du *résultat* n'en dépend, et le test le mesure à trois cadences.
- **Les effets des pas.** `0x8226E158` fait bien plus que déplacer un curseur ;
  seule la partie que le curseur voit est portée.
- **Les virtuelles `+0xB0` et `+0xBC`** du gestionnaire de mission, appelées par
  `0x82266710`. Le champ qu'il écrit est reproduit ; ce que les deux appels font
  ne l'est pas.
