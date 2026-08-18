# La file se remplit, et n'est jamais déclenchée

Date : 2026-08-18

## Correction de mon propre commit `7ac891c7`

J'y ai écrit : « rien de ce que le frontend calcule n'atteint l'anneau ». La
mesure ne portait que sur `graphics.ring.submissions`, et ce compteur compte
des **coups de pointeur d'écriture**, pas des octets produits — le test
`ac6-demo-core-tests` le montre : trois `apply_xenos_mmio_write(0x7FC80714,…)`
donnent `submissions == 3`.

Avec les bons témoins (`AC6_DEMO_WATCH_IB_WRITERS`, `AC6_DEMO_WATCH_RING_KICK`),
run START de 4 000 ticks :

```text
AC6_IB_WRITE       163 933 écritures invitées dans le buffer indirect
  [0..499]    12903      [2000..2499]  20559
  [500..999]  21112      [2500..2999]  21744
  [1000..1499] 21386     [3000..3499]  21750
  [1500..1999] 22735     [3500..3999]  21744
  dernière écriture : tick 3981

AC6_RING_KICK      4 événements, TOUS au tick 0
  lr=0x821C4A28  wptr 0x00 -> 0x16 -> 0x19
  lr=0x821C5C98  wptr 0x00 -> 0x16 -> 0x19
```

Le guest remplit donc des listes de commandes **à cadence constante jusqu'au
dernier tick**, et ne déclenche l'anneau qu'au tick 0. Ce n'est pas « rien
n'atteint l'anneau » : c'est **la file se remplit et n'est jamais déclenchée**.
Deux défauts différents, deux corrections différentes.

## Le verrou, énuméré exhaustivement

Le rapport `infos` écrivait « son **unique writer qualifié** » — formule qui
dit « celui que nous avons qualifié », pas « le seul de l'image ». Balayage
exhaustif du C++ généré sur le déplacement 21600 :

```text
ÉCRIVAINS de [device+0x5460] : 2 sites, 1 fonction
  sub_821ADAB8   non atteint (neutre et START)
LECTEURS : 1
  sub_821C57D0   atteint, 11 863 fois
```

Un seul écrivain dans toute l'image, et il ne tourne pas. Le lecteur tourne une
fois par présentation, lit 0, et repart sans déclencher.

## Et le callback est bien enregistré

```text
0x821ADC78  enregistreur du callback   atteint,  1 fois
0x821C64E8  owner / initialiseur       atteint,  1 fois
0x821C57D0  soumetteur                 atteint, 11 863 fois
0x821ADAB8  callback                   JAMAIS
0x820A4778  lève l'événement (17,6)    JAMAIS
0x820A45E0  CX360UnitManager slot +0x14 JAMAIS
```

L'enregistrement a lieu ; l'invocation n'a pas lieu, faute d'événement ; et
l'événement n'a pas lieu parce que `CX360UnitManager` n'est jamais construit.
La chaîne complète tient, du remplissage jusqu'à l'objet manquant.

## Deux points réglés au passage

**Le film a bien changé d'écran.** Slots de `CSwgRenderer` : +0x0C, +0x10,
+0x14, +0x18 sont **non atteints en neutre, 8 964 appels chacun avec START**,
du tick de l'appui au dernier. Quatre points d'entrée de rendu qui s'allument
à l'appui et ne s'éteignent plus, c'est un nouvel écran avec de nouvelles
passes. L'absence de `menu_endMode` est donc normale, et le frontend est sain.
Je laissais cette lecture ouverte dans `5dc58584` ; elle est tranchée.

**L'octet `CSwgCallback+9` n'a jamais été levé**, et pas seulement « vu à 0 » :
il est posé puis consommé dans le même tick par `sub_820CE368`, donc un
échantillonnage par tick ne pourrait pas les distinguer. Le témoin correct est
le LR : aucune arête avec `lr ∈ {0x820CE44C, 0x820CE468, 0x820CE47C,
0x820CE4F0}` dans l'un ou l'autre run. Le bloc gardé ne s'est jamais exécuté,
et `0x820EB108` — le slot +0x44 du renderer, le flip — est non atteint.

## Non établi

- Ce qui devrait construire `CX360UnitManager` hors mission, ou si un autre
  producteur devrait lever `(17,6)` pour le frontend.
- Si `0x820A4778` est bien le seul site levant `(17,6)` : hérité, pas
  re-vérifié exhaustivement ici, et la remarque du rapport `infos` sur les
  constantes voisines s'y applique.
