# AC6 — qualification de provenance du candidat vtable NDXR

Date : 2026-07-17 (Europe/Paris)

## Motif de la correction

La passe précédente (`cycle-141-ndxr-vtable-slots.md`) formulait comme
concrète la liaison entre la table `0x8205c980`, l'owner `r31` des appelants du
worker et le slot `+0x5c`. Une relecture de l'ABI statique montre que cette
formulation était trop forte.

## Faits qui restent établis

La lecture headless du `default.xex` Xbox 360 PAL (SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`) établit :

- un initialiseur qui calcule `0x8205c980` et l'écrit à l'offset zéro de son
  propre `r3` autour de `0x820f9dfc` ;
- dans la table située à `0x8205c980`, le mot `+0x5c` vaut `0x82101be0` et le
  mot `+0x13c` vaut `0x821002f0` ;
- la même table contient aussi les deux routines appelantes à des slots
  distincts : `0x8205ca8c` (`+0x10c`) -> `0x820fbc28` et `0x8205ca90`
  (`+0x110`) -> `0x820fa9c0` ;
- `0x82101be0` exécute exactement
  `lhz r11,0x1c(r4); add r11,r11,r4; lwz r3,0x8(r11); blr` ;
- `0x821002f0` efface le bit 4 du mot à `owner+0x08` lorsqu'il est appelé avec
  un owner dans `r3`.

La présence des deux routines appelantes dans cette même table est un
`cross-match` statique fort pour leur appartenance à la même famille de
méthodes. Elle ne prouve toutefois pas, à elle seule, la valeur du vtable
chargé par chaque instance dynamique au moment du worker.

## Contradiction ABI à conserver ouverte

Dans le worker, avant le dispatch indirect, l'instruction observée est :

```text
rlwinm r4,r31,0x10,0x17,0x1f
```

Avec la sémantique PPC de `rlwinm`, cette forme produit le champ numérique
`(r31 >> 16) & 0x1ff`, soit un champ de 9 bits. Elle ne suffit donc pas à
établir que `r4` est directement un pointeur vers l'enregistrement que la
feuille `0x82101be0` déréférence à `r4+0x1c`.

La bonne formulation est désormais :

```text
contrat de la feuille candidate :
  si r4 désigne un enregistrement adressable,
  r3 = mot32be(r4 + u16be(r4+0x1c) + 0x08)

provenance de r4 au worker :
  champ 9 bits extrait de r31, encodage non résolu
```

Il est interdit de transformer cette contradiction en preuve d'un pointeur,
d'un index de table ou d'un rôle NDXR sans donnée supplémentaire. La nature de
`r31`, le vtable effectivement chargé par l'owner dynamique et la convention
d'encodage de ce champ doivent être corrélés par d'autres preuves statiques ou
dynamiques.

## Niveau de confiance corrigé

`confirmed` :

- les octets et les contrats locaux de `0x82101be0` et `0x821002f0` ;
- les deux mots présents dans la table candidate à `0x8205c980` ;
- l'extraction d'un champ de 9 bits dans le worker.

`cross-match` ou `unknown` :

- appartenance des fonctions `0x820fa9c0` et `0x820fbc28` à la même famille de
  méthodes que la table candidate (`cross-match`) ;
- identité de l'instance construite autour de `0x820f9dc8` par rapport aux
  owners `r31` des appels `0x820fbbd4` et `0x820fcf3c` ;
- utilisation effective de `0x8205c980` par ces appels ;
- interprétation de `r4` comme pointeur, index ou valeur encodée ;
- relation métier avec NDXR, scène, rendu ou vol.

## Suite sans action humaine

Suivre d'abord les valeurs et les writers de `r31`/`r4` dans les deux
appelants, rechercher d'autres tables possédant les mêmes corps de slot, puis
comparer les vtables chargés dynamiquement lorsque les artefacts le permettent.
Une session humaine n'est pas nécessaire à ce stade ; la prochaine décision
doit rester fondée sur une preuve de provenance ou être explicitement laissée
ouverte.
