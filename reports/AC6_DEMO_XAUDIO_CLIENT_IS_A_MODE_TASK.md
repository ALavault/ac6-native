# La « frontière XAudio » est une tâche de mode, pas une couche audio

Date : 2026-08-17
Portée : démo PAL `Default.xex` `de917873…405da8`, projet `ghidra-projects/ace-combat-6-demo`
Instrument : `tools/whose_vtable.py` sur l'image à plat `.build/Default.xex.base.bin`

## Ce que je corrige

Le commit `33f975bd` décrit l'objet `0x1005CEBC` comme « le client XAudio », sa
vtable `0x820653BC` comme « absente de l'atlas RTTI » et en conclut que « cette
couche dispatche à la C » et que « la classe ne peut pas être nommée depuis les
données de type du binaire ». **Les trois affirmations sont fausses.**

`0x820653BC` n'est pas le début d'une vtable. C'est le **slot `+0x48` de la
vtable `0x82065374`**, et cette vtable appartient à `CModeTaskRanking` :

```text
$ python3 tools/whose_vtable.py .build/Default.xex.base.bin 0x82356040
0x82356040 appears in 1 aligned word(s) outside .pdata: 1 named, 0 unnamed
    at 0x820653BC   vtable 0x82065374 slot +0x48   CModeTaskRanking  [class-map]
```

L'objet porte donc un pointeur de vtable de **sous-objet de base** : héritage
multiple, dont l'une des bases est l'interface de client de rendu audio. Ce
n'est pas une couche C sans types ; c'est une classe C++ nommée que l'atlas
connaît, et que j'ai cherchée au mauvais endroit — j'ai traité une adresse
interne à une vtable comme si elle en était le début.

La fonction fautive relève de la même classe :

```text
$ python3 tools/whose_vtable.py .build/Default.xex.base.bin 0x82354390
    at 0x82065338   vtable 0x82065324 slot +0x14   CModeTaskRanking  [class-map]
```

`0x820653A8`, la vtable intermédiaire que pose le constructeur `sub_82355F70`
avant de la remplacer par `0x820653BC`, est le slot `+0x34` de la même vtable —
la séquence normale d'un constructeur qui installe la base puis la dérivée.

## Pourquoi je m'étais trompé

J'ai déroulé le `RTTICompleteObjectLocator` à la main, avec de mauvais
décalages de champs, et j'ai lu des noms vides et un descripteur hors image.
`whose_vtable.py` existe exactement pour cela : sa propre docstring dit que la
marche manuelle a été faite « quatre fois, chaque fois un peu différemment, et
une fois à tort ». Je l'ai refaite une cinquième, à tort. L'outil répond en une
commande et sur la même image.

## Ce que cela change pour le plan

`CModeTaskRanking` est une **tâche de mode**, de la famille des écrans de
frontend, pas un service audio. La sonde par défaut construit désormais aussi
`ACE6::CAce6TaskNode` et `ACE6::CAce6TaskManager` (vtables `0x8201307C` et
`0x82013084`, nommées par leurs descripteurs `0x823C2800` et `0x823C2824`), à
côté des trois tâches historiques.

Le sous-système de classement vient par ailleurs de recevoir sa fermeture
statique côté `infos` (`reports/AC6_DEMO_RANKING_BOARD_STATIC_CLOSURE.md`,
`analysis/ac6-demo-ranking-board-20260817/`), qui décrit la machine à états
d'écran, la topologie RTTI et le contrat de rangée. Les deux moitiés — une
tâche de mode atteinte dynamiquement, sa description statique — portent sur le
même objet et n'avaient pas été rapprochées.

## Ce qui reste non établi

- Les propriétaires de menu `0x82170F58` et `0x82185198` ne sont toujours pas
  construits, et `frontend` reste `false` avec 5 463 PRESENT.
- Lequel des mots nuls `+0x0C..+0x28` du sous-objet est déréférencé, et par
  quel chemin, reste ouvert ; ce qui est acquis est qu'il faut le chercher dans
  `CModeTaskRanking`, pas dans une couche audio anonyme.
- Que `CAce6TaskNode`/`CAce6TaskManager` soient une nouveauté de la route
  actuelle, ou qu'ils aient toujours été là sans être rapportés par le cycle
  1778, n'est pas démontré ici.
