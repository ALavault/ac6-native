# Cycle 440 — état des lieux, et limite atteinte par la méthode employée

## 1. Recherche du cycle : non concluante, et prévisiblement

Recherche de lectures aux décalages 20, 24 et 36 dans `sub_821C3690` et
`sub_821CA6C8` : zéro dans les deux.

**Sans valeur.** Ces décalages appartiennent à l'objet *manette*, pas à l'écran.
L'écran y accèderait par un pointeur et une base différente. Le motif ne pouvait
donc rien trouver, quel que soit le code — c'est le même piège qu'aux cycles 436
et 439, reconnu cette fois avant de conclure, mais reproduit quand même.

## 2. Ce qui est mesuré et tient

| fait | cycle |
|---|---|
| l'entrée arrive à l'invité, masques exacts | 401 |
| fronts d'appui et répétition corrects pour A comme pour Gauche | 427 |
| la navigation agit (bande 131 contre bruit ~3) | 421 |
| **la validation ne fait rien** (2,9 contre 3,7 de bruit) | 421, 423 |
| le sélecteur de périphérique s'est terminé, `device_id = 1` accepté | 437 |
| l'état de l'écran est **0**, au repos | 437 |
| écran identifié : objet `0xA3317DE0`, classe `0x820679A0` | 438 |
| méthodes réelles : `0x821C3690`, `0x821CA6C8` | 439 |
| l'invité présente à 60 Hz, ~56 tracés par trame | 402, 407 |

Deux défauts réels corrigés en chemin — file de frappes sans producteur (422),
`packet_number` toujours incrémenté (423) — aucun ne débloque P1.3.

## 3. Ce qui a été réfuté, pour l'essentiel par moi

textures ; navigateur non soumis ; échelle NDC nulle ; absence de boucle
d'entrée ; attente asynchrone ; machine à états qui tourne ; durée d'appui ;
stockage absent ; énumération vide ; inversion A/B ; `GetKeystroke` ;
`packet_number` ; répétition contre fronts ; routine de complétion manquante ;
état bloqué à 2 ; lacune de codegen.

Seize hypothèses, toutes écartées par mesure.

## 4. Limite atteinte

Les quinze derniers cycles suivent le même schéma : une lecture de code produit
une hypothèse, la mesure la réfute, une recherche textuelle échoue faute de
motif adéquat. Le rendement est nul depuis le cycle 421 — dernier apport réel,
la métrique par région.

La cause est identifiable : **je cherche par motif textuel dans un code
recompilé où les adressages sont calculés**, ce qui ne peut pas marcher de façon
fiable. Cinq faux départs viennent de là.

## 5. Ce qui marcherait

Une surveillance mémoire en écriture sur le champ de sélection du dialogue —
celui qui bascule de NO à YES, seul effet observable d'une entrée — et le relevé
du `lr` de l'écrivain. Cela nomme le code du dialogue **sans dépendre d'aucun
motif**, et de là on remonte à ce qui devrait réagir à A.

C'est la seule approche non encore essayée qui soit indépendante de la forme du
code.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
