# AC6 — état borné de la cible directe `0x8222CCD0`

Date : 2026-07-16

## Identité et preuves

- Cible : AC6 PAL `default.xex`, Xbox 360 ; pas Xbox One.
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Fonction : `0x8222CCD0..0x8222CE0B`, 79 instructions.
- Inspection headless lecture seule :
  `.build/ac6-ghidra-cycle-62/inspect.log`.
- Frontière précédente :
  `workspaces/ace-combat-6/reports/cycle-59-traversal-direct-target-boundaries.md`.

## Effets qualifiés

Le corps corrigé montre, sans prototype fiable, les stores suivants sur la
base implicite `r31` :

- `+0x84c` reçoit le bit numérique `0x200` de `r28 +0xe50` ;
- `+0x868` et `+0x86c` reçoivent respectivement `0x200` et `0x80` de
  `r28 +0xe44` ;
- les valeurs flottantes à `+0x838/+0x83c/+0x840/+0x844` sont comparées en
  valeur absolue avec un seuil constant ;
- selon les branches et le bas octet de `r26`, `+0x848` reçoit `r26` ou `1` ;
- la branche de remise à zéro écrit `f28` à `+0x830/+0x834/+0x840/+0x844` ;
- `+0x85c` reçoit une normalisation 0/1 du bit numérique `0x1000` de
  `r28 +0xe44`.

La fonction native `apply_function_8222ccd0_state` conserve ces opérations
avec des entrées explicites, sans nommer l'objet, les flags, le seuil ni les
registres externes. Elle modélise également le résultat de branche de
`fcmpu` : NaN n'est pas « inférieur » au seuil.

## Limite

L’appel depuis `0x8226ECB0` ne qualifie pas le propriétaire de `r31`, ni les
provenances de `r26`, `r28` et `f28`. La fonction pure n'est donc pas câblée
comme remplacement de callback de traversal. Elle ne prouve aucune sémantique
de mission, d’avion ou de spawn.

## Validation

Le test dédié couvre une remise à zéro, la conservation de la valeur `r26` et
le cas NaN. Le corpus AC6 complet reste requis avant livraison de la tranche.
