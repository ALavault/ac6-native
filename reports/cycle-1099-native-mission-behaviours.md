# Cycle 1099 — les comportements retail, en code natif

Date : 2026-08-08. Seconde tranche de décompilation native : après la lecture
du conteneur (cycle 1098), le portage de ce que les consommateurs *font*.

## Ce qui est porté

`include/ac6/retail_mission_state.h`, `src/retail_mission_state.cpp`.

**La classification de `0x820A7070`.** L'octet de classe donne la catégorie de
base (le `switch` que `0x820A7F48` implémente) ; quand le mode de jeu atteint la
table de factions, l'octet `+0x2C` de l'entrée sélectionne à la fois le mot de
drapeaux de camp et une catégorie révisée. Le portage garde **l'asymétrie du
`switch` à neuf voies** :

| code | drapeaux | catégorie |
| ---: | --- | --- |
| 0, 3 | `0x20000000`, `0x40000000` | 2 si l'ordinal est celui du joueur local, 1 sinon |
| 6 | `0x80000000` | **5** — ne consulte pas la table de 16 entrées |
| 1, 4, 7 | par groupe | 6 |
| 2, 5, 8 | par groupe | 4 |

Le code 6 casse la régularité des trois groupes. C'est ainsi dans le retail, et
c'est écrit ainsi dans le portage, avec un test qui l'épingle.

**L'ordinal, pas l'index.** La première rédaction de ce portage passait l'index
d'enregistrement au test « est-ce le joueur local ». C'est faux : le retail tient
**deux compteurs courants distincts** — un pour le code de camp 0, un pour le
code 3 — et compare l'*ordinal dans sa branche* contre une table de 16 entrées.
Sur la Mission 01 les deux coïncident, puisque toutes les factions portent le
code 0 ; ailleurs, non. `LocalPlayerSlot{branche, ordinal}` dit ce qui est
comparé.

**L'insertion de `0x8226FEC0`.** Une table de 256 emplacements avec son compte,
telle que le constructeur de base la dispose. L'insertion écrit puis incrémente.

**Le séquenceur de sous-missions** de `0x8226E908` / `0x8226E158` /
`0x82267008` : borne l'index par le compte analysé, remet l'index de pas à
zéro, horodate le démarrage dans le tableau dimensionné par ce même compte,
répond au test de durée écoulée, et évalue la condition de l'étiquette 7 — les
identifiants 0 et `0xFFFF` valant « pas de condition » **avant** tout indexage,
puis les trois opérateurs, puis le saut vers un index de sous-mission.

## Les deux divergences, assumées et testées

1. **La table refuse la 257ᵉ insertion.** Le retail ne vérifie rien : au-delà de
   256 il écrirait sur les deux pointeurs de prédicat en `+0x404`/`+0x408`, puis
   sur son propre compteur en `+0x40C`. Un produit natif ne reproduit pas une
   corruption mémoire ; il refuse et le test l'exige. La Mission 01 ne s'en
   approche pas : 230 entrées, 26 emplacements de marge.
2. **Le séquenceur rend un statut** au lieu d'appeler le pas suivant lui-même.
   Le flot de contrôle appartient à l'appelant.

Tout le reste est reproduit tel quel, y compris ce qui se lirait mieux
autrement.

## La vérification

`tests/retail_mission_state_tests.cpp`, deux moitiés comme au cycle 1098.

Sur des valeurs choisies : les trois mots de camp, l'asymétrie du code 6, le
refus d'une catégorie pour un octet de classe ou un code hors domaine, le
remplissage exact des 256 emplacements et le refus qui suit, et les trois
opérateurs de la condition avec les deux identifiants sentinelles.

Sur la charge utile retail, quand elle est présente localement :

```
enregistrements construits        230
compte de la table                230   (26 emplacements de marge)
recensement par faction     140 / 42 / 48 / 0
drapeaux                    0x20000000 pour les 230
catégories                  2 pour l'ordinal du joueur, 1 pour les 229 autres
sous-missions               4, puis Finished
```

Le recensement par faction n'est plus lu dans les données : il est **produit en
faisant tourner le consommateur porté**, et il retombe sur la distribution que
le statique avait relevée.

## Ce que cela n'établit pas

- Rien de neuf sur le retail. Ce cycle traduit ; les faits sont ceux des cycles
  1096 et 1097.
- La catégorie révisée n'est atteinte que dans les modes 2 et 3, ce qui reste un
  état d'exécution non tranché ; le portage l'expose en paramètre plutôt que de
  le supposer.
- Les objets construits sont des enregistrements plats : ni les tailles
  d'allocation par catégorie (`0x100`, `0x230`, …), ni les quinze constructeurs
  de `0x820A8138`, ni la ressource `+0x15C` ne sont modélisés.
- La table de 16 entrées du joueur local n'est pas portée ; son contenu est un
  état d'exécution, et le portage prend l'ordinal en entrée.

## Suite

Le port qui manque encore est celui des dix lecteurs `*Bin` eux-mêmes — l'image
mémoire octet pour octet, avec ses quatre particularités de dimensionneur. Sa
vérité de terrain existe déjà : 138 comparaisons `pair_equal`.
