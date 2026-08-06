# Cycle 1043 — frontière d’entrée du créateur d’unités

Date : 2026-08-06.

## Provenance

- Lane : bridge, instrumentation en lecture seule; cette note ne peut pas
  valider un gate natif.
- XEX PAL : `game-files/default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Source externe : commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, arbre
  dirty; le fichier de probe n’est pas copié dans le dépôt natif.
- Exécutable instrumenté pendant les runs : SHA-256
  `d6cae44a4981af1e5bd5d2231a0649ef0bcff8b4df0b9c78a7aec1a6179ba7a5`.
- Source du probe pendant les runs : SHA-256
  `e5dae20f99e81fcef4a0cd579b906c90b0deedc169142b3ab91e354e4f654672`.
- Les runs utilisaient `SDL_AUDIODRIVER=dummy`; le GPU hôte observé est
  `NVIDIA RTX PRO 4000 Blackwell`.

## Expérience bornée

Les trois routes ont utilisé des copies de profils temporaires et le même
hook strictement read-only sur `0x820A7F48` ainsi que sur ses constructeurs
directs `0x822A6560`, `0x822A8570` et `0x820A8E08`.

| run | route | frontière atteinte | log SHA-256 |
|---|---|---|---|
| 1041 | profil gameplay existant | `selector44=3`, puis `state40=8 selector44=4 type28=6`; `type=6 → type=8` non franchi | `5bba586688d52633bb86af10234c36ec9648b7622121890f7668f4e08ec69447` |
| 1042 | profil frais, `Left+space` | `type28=6`; aucun passage à `type28=8` | `cacbce2eb4e9be1c1d3f684c0b33562c5dd07b8d214a1e01d3bbfb7ed347af52` |
| 1043 | profil frais, `Left+space` puis pulse `space` | `state40=8 selector44=4 type28=6`; le browser reste sans réponse | `996e6907d77cbf20bdd3c46fff40210ae3b5b684e5ad6aaad751ec82b14097e5` |

Les trois logs contiennent zéro ligne
`[ac6-gameplay-unit-factory]` et zéro ligne
`[ac6-gameplay-constructor]`. Ils ne contiennent donc ni appel, ni argument,
ni résultat permettant d’attribuer une unité retail ou une vague.

## Décision

Cette expérience ne réfute pas le créateur retail; elle qualifie seulement une
précondition manquante : la route bridge ne rejoint pas le HUD gameplay avec
la frontière de stockage `type28=6` fermée. Le census gameplay du cycle 1035
reste la seule preuve dynamique qualifiée et ne doit pas être réinterprété.

Aucun gate J1 n’est promu. La prochaine acquisition bridge doit d’abord fermer
la transition `state40=8, selector44=4, type28=6` avec un artefact d’entrée
hashé; elle ne doit pas relancer un census générique ni les hooks de
constructeurs tant que cette transition n’est pas observée.
