# Cycle 413 — ce que je suivais est la couche manette, pas l'interface

## 1. Bogue corrigé dans ma propre sonde

Le sélecteur n'est **pas** le `r11` d'entrée. L'aiguilleur le charge depuis
`[this+8]` après son prologue (`generated/ac6recomp_recomp.38.cpp:23531`). Ma
sonde lisait `ctx.r11` à l'entrée — la valeur résiduelle de l'appelant. D'où le
résultat uniforme du cycle 412, qui ne mesurait rien.

Corrigée pour lire `[this+8]` en mémoire invitée.

## 2. Mesure, désormais interprétable

| objet | état | branche | appels (marche) | appels (bloqué) |
|---|---|---|---|---|
| `0x8290DE3C` | −1 puis **0** | `sub_8234D478` puis **poll** | 1 puis 1049 | 1 puis 3299 |
| `0x8290DEC4` | −1 | `sub_8234D478` | 1050 | 3300 |
| `0x8290DF4C` | −1 | `sub_8234D478` | 1050 | 3300 |
| `0x8290DFD4` | −1 | `sub_8234D478` | 1050 | 3300 |

Un seul objet passe à l'état 0 et scrute ; les trois autres restent à −1
indéfiniment.

## 3. Réinterprétation : ce n'est pas l'interface

Quatre objets espacés de 0x88, dont **un** actif et **trois** figés dans un état
de reprise : c'est la forme des **quatre emplacements de manette**. `[this+8]`
est l'état de connexion — −1 pour un emplacement vide qui retente, 0 pour la
manette branchée que l'on scrute.

Donc `sub_8234D510` est le gestionnaire **par manette**, pas par écran. Les
cycles 408 à 412 ont caractérisé la plomberie XInput, pas la machine à états de
l'interface. L'hypothèse du cycle 409 — un gestionnaire d'écran atteint
indirectement — visait la mauvaise couche ; l'indirection observée est celle du
tableau des emplacements.

Et le comportement est **identique** avant et pendant le blocage, ce qui est
cohérent avec tout le reste : la couche d'entrée est saine de bout en bout.

## 4. Bilan honnête de cinq cycles

Les cycles 408-413 ont produit : une sonde correcte, deux bogues d'instrument
trouvés et corrigés (registre lu au mauvais moment ; arbre de code périmé), et
la certitude que la couche manette n'est pas en cause. Ils n'ont **pas**
rapproché de la mission 1.

La leçon récurrente, désormais à sa troisième forme : je bâtis sur des
interprétations avant de les avoir vérifiées. Ici, « quatre objets appelés
indirectement » a été lu comme « écrans » alors que « emplacements de manette »
était au moins aussi probable, et une seule mesure du contenu de `[this+8]`
l'aurait tranché d'emblée.

## 5. Direction correcte

L'interface est ailleurs. Le point d'entrée n'est pas le lecteur de manette mais
le consommateur du masque de boutons **au-dessus** de lui. Le chemin praticable
reste celui laissé de côté : comparer avec l'oracle Xenia le comportement de ce
même écran, pour savoir ce que le jeu est censé y faire.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
