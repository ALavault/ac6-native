# Cycle 1798 — sous-enfant type 4, liste 13

Le gate joint exactement la première frame de l'enfant post-START à son
sous-enfant : le type 5 de `0x2E3F8C50` porte l'index objet `0x42` et crée
`0x2E3F1350` avec `D=0x2DD79358`. Sa frame 0 contient un unique type 4 dont
le `list_index` vaut 13.

Aucun offset VM n'est exécuté par cette branche au tick 3001. La frontière se
déplace vers la chaîne graphique `0x82326608 -> 0x82326420` : résolution de la
liste 13, record et premier draw/consumer renderer persistant.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

