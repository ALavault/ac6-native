# AC6 cycle 203 — contrat de reset et de propagation scalaire

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule sur le projet Ghidra canonique. Aucun
état Ghidra, binaire ou artefact généré n'a été modifié.

## Deux chemins convergents

Les islands autour de `0x8226a310` et `0x8226b418` montrent deux chemins
d'initialisation distincts qui partagent le même contrat de remise à zéro :

- l'objet reçu dans `r3` conserve ses champs d'état autour de `+0x140` et
  `+0x170` ;
- le quadruplet global `0x823fb360` reçoit `+0x34 = 0`, `+0x10 = 0`,
  `+0x3c = -1` et `+0x40 = 0` ;
- le chemin `0x8226a310` recopie `objet+0x1a4/+0x1a8/+0x1ac` vers
  `objet+0x190+0x04/+0x08/+0x0c` et nettoie les bits de contrôle associés ;
- le chemin voisin `0x8226b418` applique la même famille de masques à
  `objet+0x170` avant de remettre le contexte global à zéro ;
- les deux chemins chargent ensuite la collection située dans le même slot de
  table (`0x02009fc8`, via le pointeur global de table) et appellent
  `0x8226ecb0` avec `f1 = 0.0`.

Cette répétition est une preuve de contrat de cycle, pas une preuve du rôle
gameplay de l'objet.

## Broadcast `0x8226ecb0`

Le helper reçoit `r3 = collection` et `f1 = scalaire`. La collection possède
un compteur en `+0x04` et un tableau de pointeurs à partir de `+0x08` :

```text
collection +0x04 : count
collection +0x08 : entries[count] (pointeurs)
```

Pour chaque entrée non nulle, le helper lit `entry+0x118` et ne poursuit que
si l'une des familles de bits est active (`0x022` ou `0x402`). Les branches
sélectionnent alors des slots virtuels de l'élément (`+0x54`, `+0x5c`,
`+0x60`, `+0xcc`) et, selon leur résultat, transmettent le scalaire à des
handlers qui reçoivent aussi `entry+0x10`, `entry+0x24f8` ou `entry+0x254c`.

Le contrat sûr est donc :

```text
scalar_reset_broadcast(collection, scalar, eligible_entry_flags)
```

Il ne faut pas encore appeler ces entrées `aircraft`, `camera`, `velocity` ou
`mission objects`. Les vtables et les offsets sont confirmés ; la sémantique
fonctionnelle reste `cross-match`/`needs-dynamic-evidence`.

## Appelants confirmés

Le balayage PPC brut trouve exactement deux appels directs au helper :

- `0x8226a508 -> 0x8226ecb0`, depuis le chemin de reset autour de `0x8226a310` ;
- `0x8226b5bc -> 0x8226ecb0`, depuis le chemin de reset autour de `0x8226b418`.

Les deux transmettent la constante flottante nulle. Cela renforce le lien
entre la remise à zéro locale, le contexte global et la propagation vers les
éléments éligibles.

## Qualification

- `confirmed` : deux appelants, collection `+0x04/+0x08`, filtres de flags,
  slots virtuels, scalaire nul et reset du contexte global ;
- `cross-match` : contrat commun de cycle et propagation vers un sous-système
  d'éléments ;
- `unknown` / `needs-dynamic-evidence` : identité gameplay des éléments et
  signification du scalaire.

Aucune intervention humaine n'est requise pour cette passe.

## Validation

- `DumpRange.java 0x8226a3e0 0x8226a520` ;
- `DumpRange.java 0x8226b3e0 0x8226b610` ;
- `DumpRange.java 0x8226ec00 0x8226ede4` ;
- `FindPpcRawBranchesTo.java 0x8226ecb0` ;
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6` ;
- mode : `-readOnly -noanalysis`.

