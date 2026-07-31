# Cycle 423 — second défaut réel corrigé, toujours pas la cause

## 1. Le défaut

`packet_number_++` s'exécutait à **chaque** appel de `GetState`, sans condition.

Le contrat XInput est l'inverse : `packet_number` ne change que si l'état change,
de sorte qu'un appelant peut comparer deux relevés pour savoir si quelque chose a
bougé. En incrémentant toujours, le champ annonçait un changement à chaque
scrutation et **ne transportait aucune information**.

## 2. La correction

Comparaison de la charge utile complète — boutons, gâchettes, deux sticks — avec
le relevé précédent ; `packet_number_` n'avance que si elle diffère. Sept
membres ajoutés pour mémoriser l'état précédent, afin que le mouvement des
sticks et des gâchettes compte aussi, et pas seulement les boutons.

## 3. Résultat : toujours pas la cause

| transition | bande |
|---|---|
| Gauche | **131.070** |
| valider (A) | 2.984 |
| repos (témoin) | 3.654 |

L'écart de validation (2,98) est **inférieur au bruit de repos** (3,65). Aucun
effet, sans ambiguïté.

## 4. Deux corrections, deux résultats négatifs

| cycle | défaut corrigé | réel ? | débloque P1.3 ? |
|---|---|---|---|
| 422 | file de frappes sans producteur | oui | non |
| 423 | `packet_number` toujours incrémenté | oui | non |

Les deux sont des écarts objectifs au comportement attendu du pilote, corrigés
et conservés à ce titre. Aucun des deux ne touche au blocage.

Je note l'enchaînement sans le maquiller : j'ai proposé deux causes plausibles,
je les ai corrigées proprement, et la mesure a réfuté les deux immédiatement.
C'est un meilleur régime que les cycles 394-420 — les hypothèses sont désormais
testées dans le cycle même où elles naissent — mais le livrable n'avance pas.

## 5. Le fait central, inentamé

Sur le sélecteur de périphérique, la croix directionnelle agit (131 contre ~3 de
bruit) et les boutons de face n'agissent pas, alors que les deux proviennent du
même `X_INPUT_STATE`, correctement rempli et correctement daté.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
