# Calibration de l'apparieur de bibliothèque, et une affirmation retirée

Date : 2026-08-18

## Ce que j'ai publié à tort dans `1a5c4f76`

> « Ce sont donc du code du jeu […] `0x821AD378` […] c'est du code Namco, pas
> du D3D. »

Cette conclusion vient d'un **négatif** — « aucune correspondance » — tiré d'un
instrument dont les positifs confirmés descendent à **1/16**. C'est précisément
le raisonnement que ce dépôt refuse.

Et la géographie le contredisait déjà : `0x821AD378` se situe **entre**
`0x821ACCD0` et `0x821ADAB8` (`D3D::CounterHandler`, 14/24), au milieu d'une
plage dont les deux bornes confirmées sont `0x821ADAB8` et `0x821C57D0`. Une
fonction posée entre deux fonctions de bibliothèque est vraisemblablement de la
bibliothèque.

## Le plancher de bruit, mesuré

Fenêtre de 16 octets, 60 fenêtres, sur `sub_8218A7A8` —
`CModeTaskTitle::update`, du code Namco certain, lu et cité dans plusieurs
rapports :

```text
0x8218A7A8  D3DStateBlock_Release              d3d9.lib   3/60
0x8218A7A8  D3D::XMemAlloc_MemAlloc            d3d9d.lib  2/60
0x8218A7A8  D3D::XMemAlloc_MemAlloc            d3d9i.lib  2/60
0x820CE368  XGRAPHICS::R400SchedModel::Apply   xgraphics  1/60
0x820E8F90  aucune correspondance
```

Seize octets font quatre instructions ; n'importe quel prologue les fournit.
**Le plancher est donc autour de 3/60**, et tout ce qui reste sous ~6/60 n'est
pas distinguable du bruit.

Cela invalide, en tant qu'identifications, les scores que j'ai obtenus à
fenêtre 16 pour `0x821AD378` (1/60), `0x821AD7C0` (2/60), `0x821ACCD0` (5-6/60),
`0x821BE9A0` (3/60) et `0x821C64E8` (3/60). Aucun de ces noms n'est retenu.

## Le seul nom qui ressort

```text
0x821C5190  D3D::SwapCallback(DWORD)   24/60 dans d3d9.lib ET d3d9i.lib
```

Vingt-quatre contre un plancher de trois, dans deux archives, pour la fonction
qui lit `*VdGlobalDevice` puis prend un spinlock. C'est une identification.

## Ce qui reste vrai de `1a5c4f76`

Les huit noms obtenus à fenêtre 32 et 48 ne sont pas touchés : les fenêtres
longues n'ont pas de plancher mesurable ici, et `D3D::CBlocker::Check` à 15/24
ou `D3D::CounterHandler` à 14/24 ne sont pas du bruit. La conclusion que la
frontière `device+0x5460` est un champ de compteur de performance tient.

Ce qui tombe est uniquement la phrase sur le code Namco.

## Ce qui est corrigé dans l'outil

La calibration est désormais dans sa docstring, avec la règle qui manquait :
**un score faible n'est pas une absence.**

## Non établi

- Le nom de `0x821AD378`, `0x821AD7C0`, `0x821ACCD0`, `0x821BE9A0` et
  `0x821C64E8`. Ni « XDK » ni « Namco » n'est établi pour eux.
- Un seuil chiffré pour les fenêtres de 32 et 48 octets : le plancher n'y a pas
  été mesuré, seulement supposé plus bas.
