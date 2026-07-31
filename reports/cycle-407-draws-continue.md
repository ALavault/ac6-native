# Cycle 407 — les tracés invités continuent : la scène est re-soumise à l'identique

## 1. Mesure, avec arrivée vérifiée

| contrôle | valeur |
|---|---|
| arrivée sur l'écran | itération 11, 11:48:14 |
| toujours sur l'écran 10 s plus tard | `save-screen 5.0` ✔ |
| écart de pixels sur la fenêtre | 0.6364 |

Fenêtre journalisée exploitable (le journal tourne et n'a conservé que la fin) :
2 secondes, entièrement comprises dans l'intervalle où la présence sur l'écran
est confirmée.

| seconde | tracés invités | présentations |
|---|---|---|
| 11:48:24 | 1970 | 35 |
| 11:48:25 | 952 | 17 |
| **total** | **2922** | **52** |

**56,2 tracés par présentation.**

## 2. Réponse à la question du cycle 405

Les deux issues possibles étaient : les tracés continuent, ou ils cessent.

**Ils continuent.** L'invité soumet environ 56 tracés par trame, à 52
présentations sur deux secondes — soit ~26 Hz sur cette fenêtre. La scène est
donc **recalculée et re-soumise en permanence**, et elle produit chaque fois la
même image.

L'hôte ne se contente donc pas de représenter un dernier tampon : la boucle de
rendu invitée tourne pleinement.

Le chiffre de 56 tracés par trame recoupe celui relevé dans les cycles
antérieurs sur cet écran, ce qui est une vérification croisée indépendante.

## 3. Précision sur le « gel »

L'écart de pixels vaut ici 0.6364, non nul, alors que le cycle 404 mesurait
exactement 0.000. Les deux sont compatibles avec ce qui précède : l'écran est
visuellement stable, avec par moments une animation de fond de très faible
amplitude. Ce n'est pas une image gelée par arrêt du rendu — c'est une image
**redessinée à l'identique**.

Je corrige donc la formulation du cycle 404 : « l'invité re-présente
indéfiniment une image identique » était juste sur le fait, mais suggérait un
rendu arrêté. Il ne l'est pas ; il tourne à plein.

## 4. Conséquence pour la suite

Le défaut n'est ni dans la présentation, ni dans la boucle de rendu, ni dans
l'entrée, ni dans les fils, ni dans la transformation géométrique. L'invité
dessine activement, chaque trame, un écran dont le contenu ne change jamais et
qui ignore les boutons qu'il reçoit pourtant correctement.

Ce qui reste à lire est **l'état de l'interface dans les données invitées** :
quelle valeur la machine à états porte, et pourquoi les entrées ne la font pas
transiter. C'est un travail de rétro-ingénierie sur l'invité, pas une mesure
supplémentaire côté hôte.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
