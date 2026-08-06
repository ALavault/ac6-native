# Cycle 1050 — frontière du probe texte bridge

Date : 2026-08-06.

## Objet

Tester le logger de chaînes UI sur la source bridge courante, sans attribuer de
texte de scénario à Mission 01 avant d'avoir atteint la transition gameplay.
Les captures et logs restent sous `/tmp`; aucun payload retail n'est ajouté au
dépôt.

## Provenance

- Source externe : commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, arbre dirty.
- XEX PAL : SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- GPU : `NVIDIA RTX PRO 4000 Blackwell`; `SDL_AUDIODRIVER=dummy`.
- Recipe : `scripts/ac6-first-mission-bridge-window-boundary.steps`, SHA-256
  `df2dd2fb013b5f2c1a4b0b5a84de31c901e52d5ec87bcdac925493984cb01c65`.

## Runs bornés

Le build avec le préambule PPC forcé (`dc6f23a68fa3217e6d744a230c97da9f9e5647b2f60c39a81feeeeaf39f370e1`)
reste une A/B diagnostique : il ne rejoint pas `type28=37` après les deux
premières touches. Le logger n'a produit aucune ligne `[ac6-text*]`.

Le build isolé avec le macro/logger activé mais le préambule des unités guest
désactivé (`c94d00106afb23d105c5b3f1c24715124c1e62d01c39dd0b45cae7ffd4153a75`)
échoue au même point avant `type28=37`; son log append-only a le SHA-256
`ccd4390e290a8f372dec2302bf262e89251ddc51efca301258021b43d5d8ee5b` et publie
1 344 `PRESENT`. Aucun `[ac6-text*]`, aucune identité d'objectif et aucun
événement d'unité/vague n'est produit.

## Qualification

Résultat **non qualifié** : ces runs n'atteignent pas la frontière storage
Mission 01 et ne réfutent ni la présence de textes retail ni leur absence. Le
macro de diagnostic ne doit pas être activé dans le runtime natif, et les deux
hashes restent des supports bridge uniquement. Le prochain travail utile reste la
fermeture du handoff gameplay et l'acquisition d'une identité d'unité/vague,
pas une interprétation de chaînes non capturées.
