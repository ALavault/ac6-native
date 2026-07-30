# Cycle 335 — P1.2 fermée : l'entrée atteint l'invité et change durablement l'état

## 1. Ce que l'invité reçoit réellement

Sonde posée à la frontière qui compte — le masque de boutons écrit dans le
`X_INPUT_STATE` de l'invité par `XamInputGetState` — journalisée sur transition.

```
run C (témoin)      : buttons=0x0000 result=0x0                    (uniquement)
run T (traitement)  : buttons=0x0000 result=0x0
                      buttons=0x0010 result=0x0   <- X_INPUT_GAMEPAD_START
                      buttons=0x1000 result=0x0   <- X_INPUT_GAMEPAD_A
```

`result=0x0` est `X_ERROR_SUCCESS`. **L'invité lit les boutons pressés**, et
seulement dans l'exécution où ils l'ont été. C'est indépendant de tout
alignement d'images et de tout bruit d'animation.

Conditions nécessaires, toutes mesurées au cycle 334 : `--mnk_mode=true` (faux
par défaut), touches liées (`Escape`=Start, `Space`=A), et appui **maintenu**
(`keydown`/`keyup`, jamais `xdotool key`).

## 2. Que l'effet est durable : deux témoins contre un traitement

Discriminateur sans alignement : l'**activité temporelle**
`moyenne |image_i - image_{i-1}|` à l'intérieur de chaque exécution. Une
comparaison croisée image à image était impossible — le cycle 334 avait mesuré
un plancher de bruit témoin-contre-témoin de 116 contre un effet de 36.

| exécution | activité moyenne après pression | images figées (< 0,01) |
|---|---:|---:|
| témoin C | 19,819 | **9/12** |
| témoin C2 | 0,000 | **12/12** |
| traitement T | 3,693 | **0/12** |

Les deux exécutions **intactes** se figent sur une image parfaitement statique —
activité exactement `0,000`. L'exécution ayant reçu l'entrée ne se fige
**jamais** : elle reste animée à ~1,0 pendant les 18 s restantes.

La divergence est **durable**, **reproduite contre deux témoins indépendants**,
et concorde temporellement avec la réception de `0x0010` puis `0x1000`.

Capture finale du traitement : écran-titre « ACE COMBAT / Fires of Liberation »
vivant, 58,68 im/s, 255 869 dessins hôte. Les témoins, au même instant, sont
figés.

## 3. Verdict

**P1.2 est franchie.** L'entrée modifie l'état présenté, et l'effet persiste.

Deux lignes de preuve indépendantes le soutiennent — ce que l'invité *lit*, et
ce que l'exécution *fait* ensuite — ce qui compte, parce qu'aucune des deux
seule ne suffisait : la première ne montre pas d'effet, la seconde ne montre pas
de cause.

## 4. Ce que cela corrige

Le cycle 314 concluait « l'entrée est morte ». Trois raisons indépendantes
condamnaient ce test, toutes établies depuis : mauvaises touches, pilote
désactivé par défaut, frappe trop brève pour la cadence d'interrogation. Aucune
n'était un défaut de l'invité.

## 5. Ce qui reste

- P1.3 — traverser jusqu'au sélecteur de campagne : **non fait**.
- P2 à P7 — entrée 9, chargement de la première mission, renderer, vol, audio,
  parité, durcissement : **non faits**.

La première mission ne se joue pas. « Jouable » et « parité retail » restent
interdits. `recompiler-generated` n'est pas `verified`.

## 6. Règle ajoutée

**Quand un témoin bruite plus fort que l'effet, changer d'observable plutôt que
de conclure.** Le même protocole a rendu un verdict nul en pixels croisés et un
verdict net en activité temporelle intra-exécution, sur les mêmes données. Le
choix de l'observable, pas la mesure, décidait de la réponse.
