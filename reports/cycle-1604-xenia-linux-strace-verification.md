# Cycle 1604 — vérification du launcher Xenia Linux `907d92b`

```text
ROLE=ORACLE_RECOVERY
LANE=stock avec intervention hôte déclarée
TARGET=default.xex
TARGET_SHA256=acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
INTERVENTION=strace -f -qq -e trace=none
```

## Identité et protocole

- Binaire Canary Linux : `907d92b`, SHA-256
  `0cf3fa5b38211dcc53f774002409a149a4cdfd8e2e24f16c7ba578d767b81cad`.
- Launcher : SHA-256
  `8de67fecfe9a5c6f6901e1868e76a8590707fd45940a64d71e6585fdacfc8950`.
- Configuration portable : SHA-256
  `812edf5c8acf67547442efdcf608871a33a23a1f02726283b96d5246064f65a3`;
  un profil portable est chargé en slot 0. Aucun identifiant ou contenu de
  sauvegarde n'est publié.
- Trois copies temporaires indépendantes du bundle ont été exécutées sous
  Xvfb 1280×720, Vulkan et `SDL_AUDIODRIVER=dummy`, pendant 24 secondes.
  Captures à 8 et 20 secondes. Aucune source Xenia ou AC6 n'a été modifiée.

## Résultat A/B

| Run | Intervention | Profil | Jalon commun | Capture 20 s | Écart 8→20 |
|---|---|---:|---|---|---:|
| A | launcher `strace -f` | 1 | swapchain, module, audio, 29 threads | intro Project Aces visible | 886 282 pixels |
| B | aucune | 1 | swapchain, module, audio, 29 threads | écran figé | 0 pixel |
| C | launcher `strace -f` | 1 | swapchain, module, audio, 29 threads | intro Project Aces visible | 575 901 pixels |

Les runs positifs ont produit respectivement 50 651 et 30 251 couleurs à
20 secondes. Le contrôle sans `strace` a produit deux PNG byte-identiques,
SHA-256
`1a2eed61b14c9a9bd151f6226ceee6aad3a8fcbdb23dd2d405c1cd71ea3c01a1`.
Les logs A/B/C ont les SHA-256 suivants :

```text
d9c6820bb61c2dfb737aa64a19b81d4422df5d2492fd0d4cdeb320146b0ec832
e9c79c4adbaae6f9a6aca6915772620e09618f7bf9fc0c30a30be4fc15139381
0f1355e2599ad709459a3e531505d094f57db386f97275b213d7182a131fdc24
```

## Conclusion bornée

Le contournement est causal et reproductible pour cette machine : le même
binaire, profil et XEX progresse visuellement deux fois sous `strace`, tandis
que le contrôle direct reste figé. Cela ferme le défaut « aucun contenu
présenté sous Linux » du cycle 1603, mais ne qualifie ni le menu, ni l'input,
ni une mission, ni la cadence ou les pixels retail exacts.

`strace` intercepte les signaux de faute utilisés par Xenia et perturbe le
scheduling hôte. Il reste donc une intervention d'oracle explicitement
déclarée, jamais une dépendance du produit ou du worker loop quotidien. Les
captures, logs, profil et sauvegardes restent uniquement sous
`/tmp/ac6-xenia-907d92b-verify.4rvfV4/`. Aucun processus Xenia ni Xvfb lancé
par cette action ne reste actif ; le Xvfb partagé d'une campagne AC5 distincte
n'a pas été touché.
