# Cycle 439 — `0x82079088` n'est probablement pas du code

## 1. Le constat qui pouvait tromper

`sub_82079088`, deuxième entrée de la table virtuelle de l'écran, **n'existe pas
dans l'arbre généré**. Aucune occurrence, ni dans le code, ni dans la table des
fonctions.

Lu seul, cela ressemble à une fonction omise par le recompilateur — exactement
l'hypothèse « défaut de codegen » avancée au cycle 435.

## 2. Le contexte qui l'infirme

Étendue réelle du code généré :

| grandeur | valeur |
|---|---|
| fonctions distinctes | **23 058** |
| première adresse | `0x82090000` |
| dernière adresse | `0x823E7DF0` |

`0x82079088` est **en dessous** de `0x82090000`, donc hors de la plage
recompilée — mais il n'est pas seul : les tables virtuelles elles-mêmes,
`0x820679A0` (cycle 438) et `0x820679F4` (cycle 417), sont dans la même région
`0x8206–0x8207`.

Or une table virtuelle est **une donnée**, pas du code. Cette région est donc
celle des données en lecture seule, et non une lacune de recompilation.

## 3. Conclusion prudente

L'explication la plus simple n'est pas qu'une fonction manque, mais que **mon
interprétation des emplacements est fausse** : l'entrée 1 n'est vraisemblablement
pas un pointeur de méthode. Les tables virtuelles PowerPC comportent
couramment des entrées qui n'en sont pas — informations de type, décalages
d'ajustement.

Les entrées 0 et 2, `0x821C3690` et `0x821CA6C8`, tombent bien dans la plage du
code et **existent** toutes deux dans l'arbre généré. Elles sont cohérentes avec
des méthodes ; l'entrée 1 ne l'est pas.

Je ne déclare donc **aucune** lacune de codegen. L'hypothèse du cycle 435 reste
non confirmée, et cette mesure-ci ne la soutient pas.

## 4. Ce qui reste exploitable

Deux méthodes réelles et lisibles : `sub_821C3690` et `sub_821CA6C8`. C'est là
qu'il faut chercher le traitement de l'entrée, en ignorant l'entrée 1.

## 5. Note de méthode

Cinquième fois dans cette série qu'une absence de correspondance a failli
devenir une affirmation. La parade est la même à chaque fois et tient en une
question : **de quoi cette absence est-elle l'absence ?** Ici, d'un code qui
n'avait aucune raison d'être du code.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
