# Cycle 1090 — micro-exécution de `OrderBin::read`

Date : 2026-08-08. Suite du cycle 1089, sur le lecteur dont le comportement
varie le plus : l'union à dix étiquettes.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Aucun oracle** : ni émulateur de console, ni bridge, ni exécution du produit
  natif.

## Une faille de méthode corrigée d'abord

Le cycle 1089 détectait les écritures en pré-remplissant la destination de
poison `0xCD` et en relevant les octets qui en diffèrent. Un balayage des 2 975
ordres a fait apparaître, pour l'étiquette 0, un fragment d'écriture commençant
à l'offset `0x03` au lieu de `0x04`.

La cause n'est pas un défaut de schéma : **un octet que le parseur écrit
légitimement peut valoir `0xCD`**, et la détection par différence le compte
alors comme non écrit, ce qui coupe la plage rapportée.

L'effet était identique des deux côtés, donc les 52 comparaisons du cycle 1089
restent valides — mais la détection était fausse, et une méthode fausse qui
donne le bon résultat reste fausse. Corrigée :

- côté PPC, **deux passes d'émulation** avec des poisons différents (`0xCD` et
  `0x00`) ; un octet est écrit s'il diffère de son propre poison dans au moins
  une des deux. Aucun angle mort ne subsiste ;
- côté natif, un **masque d'écriture explicite** plutôt qu'une différence.

Les trois lots ont été rejoués intégralement avec la détection corrigée.

## Le dispatch, confirmé par exécution

32 nœuds `OrderBin`, **4 par étiquette effectivement présente** dans la
Mission 01 :

| étiq. | variante | slot écrit | prédit | pas | nœuds |
| ---: | --- | --- | --- | ---: | ---: |
| 0 | *sans nom* | `+0x04` | `+0x04` | 63 | 4 |
| 1 | **OrderDisappearBin** | `+0x08` | `+0x08` | 66 | 4 |
| 2 | *sans nom* | `+0x0C` | `+0x0C` | 132 | 4 |
| 3 | **OrderStopBin** | `+0x10` | `+0x10` | 70 | 4 |
| 5 | **OrderJumpBin** | `+0x18` | `+0x18` | 74 | 4 |
| 6 | **OrderFlagBin** | `+0x1C` | `+0x1C` | 76 | 4 |
| 7 | *sans nom* | `+0x20` | `+0x20` | 75 | 4 |
| 8 | **OrderPropertyBin** | `+0x24` | `+0x24` | 80 | 4 |

Chaque étiquette écrit **exactement** le slot que le schéma prédit, et chaque
étiquette a **son propre compte de pas**, constant sur ses quatre nœuds. La
cartographie étiquette → slot n'est plus une lecture de code : elle est
exécutée.

Les étiquettes **4 (`OrderLeadBin`) et 9 sont absentes de la Mission 01** ; les
slots `+0x14` et `+0x28` que le schéma leur assigne restent donc **non exercés**.

## L'étiquette 2 ne descend jamais, sur la totalité

Le parseur natif implémente la précondition exacte de `0x82331AD0` — mot de
données résolu, table, premier enfant présent — et **lève une exception** si
elle est satisfaite, puisque le sous-lecteur `0x82331D98` n'est pas modélisé.

Passé sur les **2 975 ordres** de la charge utile : **0 levée**. La descente de
l'étiquette 2 n'est jamais empruntée par la Mission 01, ce qui confirme par une
seconde voie le résultat négatif du cycle 1085. Le sous-lecteur reste non
modélisé, et un nœud qui descendrait échouerait bruyamment au lieu de passer
silencieusement.

Sur ces mêmes 2 975 ordres, **aucun chemin d'échec fermé ne se déclenche**.

## Résultats

| cas | nœuds | résultat |
| --- | ---: | --- |
| `OrderBin::read` `0x82331208` | **32** | `pair_equal` |
| `ObjBin::read` (rejoué) | 25 | `pair_equal` |
| `ManeuverBin::read` (rejoué) | 25 | `pair_equal` |
| `ComTblBin::read` (rejoué) | 1 | `pair_equal` |
| `ComBin::read` (rejoué) | 1 | `pair_equal` |
| **total** | **84** | **84 `pair_equal`, 0 divergence** |

`OrderBin` est passé **du premier coup**, sans la correction qu'avait exigée
`ObjBin` au cycle 1089 — parce que sa comptabilité de tampon est triviale : un
mot écrit par enregistrement, pas d'arithmétique de curseur imbriquée.

## Tests

Cinq cas synthétiques ajoutés : chaque étiquette écrit son slot, une étiquette
hors intervalle n'écrit que le mot de données, une variante nommée échoue en
fermé sur un enfant absent là où l'étiquette 0 reste silencieuse, et **un octet
écrit valant `0xCD` est toujours rapporté** — le garde-fou de la faille
ci-dessus. Suite complète :
`python3 -m unittest discover -s tools/tests` → **51 tests, OK**.

## Portée

Établi : les schémas `ObjBin`, `OrderBin`, `ManeuverBin`, `ComTblBin` et
`ComBin` reproduisent le parseur retail **octet pour octet sur 84 nœuds réels**,
avec une détection d'écriture désormais sans angle mort.

Non établi :

- la sémantique derrière les pointeurs résolus — le parseur les recopie sans les
  interpréter, et `OrderBin` ne fait pas exception : savoir qu'un ordre est un
  `OrderStopBin` reste sans information sur ce qu'il arrête ;
- les étiquettes 4 et 9, absentes de la charge utile ;
- le sous-lecteur `0x82331D98` de l'étiquette 2 ;
- `ActBin`, `SetBin`, `SubMisTblBin`, `SubMisBin` et `RadioTblBin`, validés
  structurellement mais pas micro-exécutés.

`retail_units_and_waves` et `retail_objectives` restent **ouverts**.

## Prochaine tranche

`ActBin` et `SetBin` sont les deux derniers lecteurs sur le chemin des ordres ;
les micro-exécuter fermerait la chaîne `Set → Act → Order` de bout en bout.
`RadioTblBin` est à part : c'est le seul dont le domaine ouvert correspondant
(`scenario_radio_or_subtitles`) soit déjà passé.
