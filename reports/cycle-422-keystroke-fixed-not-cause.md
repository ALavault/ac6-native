# Cycle 422 — un vrai défaut corrigé, qui n'est pas la cause de P1.3

## 1. Le défaut

`MnkInputDriver::EnqueueKeystroke` était **défini et jamais appelé** — aucun
site d'appel dans tout l'arbre. La file était donc toujours vide et
`GetKeystroke` rendait invariablement `X_ERROR_EMPTY`.

Conséquence réelle : tout jeu lisant ses validations par
`XamInputGetKeystroke` ne voyait **aucune** pression, alors que tout ce qui
passe par `GetState` fonctionnait. C'est un défaut objectif du pilote, pas une
hypothèse.

## 2. La correction

Génération des fronts dans `GetState`, par différence avec le masque précédent :
un `keydown` quand un bouton s'enfonce, un `keyup` quand il se relâche — le
comportement de l'API réelle, et non une répétition à chaque scrutation. Table
des quatorze boutons vers leurs codes `VK_PAD` (A = 0x5800 … droite = 0x5813),
plus un membre `last_keystroke_buttons_`.

## 3. Résultat mesuré : ce n'est pas la cause

| transition | bande | plein écran |
|---|---|---|
| Gauche | **131.062** | 6.838 |
| **valider (A)** | 2.843 | 0.516 |
| repos (témoin) | 1.909 | 1.305 |

La navigation fonctionne toujours (131 contre ~2 de bruit). La validation reste
**sans effet** : 2,84 contre 1,91 au repos.

Donc **AC6 ne lit pas sa validation par `XamInputGetKeystroke`**. L'hypothèse
qui a motivé la correction est réfutée par la mesure qui suivait immédiatement.

## 4. Ce que je retiens

La correction est conservée : elle répare un défaut réel et vérifiable du
pilote, indépendamment de P1.3. Mais elle **ne débloque rien**, et je ne la
présente pas autrement.

C'est la première fois dans cette série que j'écris du code de correction plutôt
que du code de sonde. Le résultat est net et négatif, ce qui vaut mieux qu'un
résultat ambigu : le chemin `GetKeystroke` est désormais sain **et** écarté.

## 5. Reste

Le blocage est inchangé et toujours aussi étroitement cerné : sur le sélecteur
de périphérique, la croix directionnelle agit, les boutons de face non, et les
deux proviennent du même `X_INPUT_STATE` correctement rempli.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
