# Cycle 1128 — 797, puis 52, puis 12 : l'instrument, corrigé deux fois

Date : 2026-08-08. Cycle autonome. Il ne trouve pas le placeur ; il rend la
recherche finie et dit où elle bute.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## Deux faux positifs, nommés

Le cycle 1126 concluait qu'un déplacement de structure n'est pas une prise. Il
avait raison pour une mauvaise raison : l'instrument était faux, deux fois.

| version | règle | résultat | ce qu'elle comptait à tort |
| --- | --- | ---: | --- |
| 1126 | tout `addi rX,rY,0x50` ou `stfs …,0x50(rY)` | **797** | `addi rX,r1,0x50` — chaque cadre de pile |
| 1127 | idem, sans `r1` | **52** | trois `stfs` du **même** registre — chaque constructeur qui met un bloc à zéro |
| ici | trois sources **distinctes** | **12** | — |

Le second faux positif se voit à l'œil une fois nommé : `0x82094560` écrit `f31`
de `+0x10` à `+0x68` sans discontinuer — c'est un `memset`, pas une position. Une
position écrit trois valeurs différentes.

Douze candidats sur tout le binaire. C'est une liste qu'on lit.

## Les deux qui tombaient dans le bon quartier

**`0x822EE7A8`**, dans le voisinage des états de mission, écrit une longue suite
de **constantes** de `+0x28` à `+0x68` en alternant deux registres — une table
d'initialisation qui passe la règle des sources distinctes sans être une
position. Troisième forme de faux positif, notée pour la suite.

**`0x822AE220`**, à côté du code de la classe d'unité, en est bien une : elle
calcule un vecteur par `vmrghw`, le range en pile, puis l'écrit en `+0x50` d'un
objet **pris dans un tableau indexé** (`r31[(base+2+i)]`), et pose un octet en
`+0x71` du même objet. Son unique appelant est `0x822B3718`. Un tableau de
sous-objets indexé, avec une matrice et un drapeau — cela ressemble à une mise à
jour d'attaches ou de pièces de modèle, **pas au placement d'une unité**, et
rien ici ne dit le contraire.

## Où l'instrument bute encore

Les scans visent `+0x50` explicitement. Or le constructeur d'unité n'écrit pas
sa transformation ainsi : il utilise des **stockages vectoriels indexés**,
`li rB,0x40 ; stvx128 vrN,rA,rB`. Un placeur qui copierait la transformation
entière depuis une base `unité+0x20` avec l'index `0x30` atteindrait `+0x50`
**sans qu'aucun de ces scans le voie**.

Deux index ont été balayés — `0x50` (26 sites) et `0x40` (78) — et pas les
autres. Les biais restants sont `0x30`, `0x20`, `0x10` et `0x00`, chacun avec sa
base décalée d'autant. C'est la prise suivante, elle est mécanique, et ce cycle
ne l'a pas prise.

## Décision de cycle

Le goal demande que les choix ordinaires se tranchent ici plutôt qu'en question.
Deux tranchés :

1. **Ne pas lire les dix candidats restants un par un.** Ils sont tous hors du
   code de mission (`0x820A`, `0x820B`, `0x820D`, `0x8214`, `0x8215`, `0x8217`,
   `0x821C`, `0x823B`, `0x823E`) et le coût d'une lecture par candidat dépasse
   celui de compléter le balayage indexé, qui peut encore en révéler.
2. **Ne rien porter de ce cycle.** Aucun fait établi ici n'a de conséquence sur
   le produit natif ; les trois scripts corrigés sont versionnés, le rapport
   nomme les trois formes de faux positif, et c'est tout ce qu'il y a à garder.

`ctest 24/24`, la porte JF reste verte.

## Ce que cela n'établit pas

- **Le placement initial**, toujours. La liste des suspects est passée de 797 à
  12 et deux sont expliqués ; les dix autres ne sont pas lus.
- **Que le placement s'écrit en `+0x50`.** Si la transformation est posée en
  bloc, l'écriture porte sur `+0x20` et le scan qui la verrait n'a pas tourné.
