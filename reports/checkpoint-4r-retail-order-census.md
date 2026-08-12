# Checkpoint 4r — recensement complet `Set → Act → Order`

Date : 2026-08-12.

## Résultat

Le parseur natif conserve maintenant chaque entrée du programme comportemental
avec `(unit_index, act_index, order_index, tag)`. Cette vue est volontairement
lossless : elle ne donne aucune sémantique au tag et n’active/désactive aucune
unité. Elle fournit seulement la clé de jointure qui manquait pour rattacher
une future commande runtime à son enregistrement retail.

Sur le payload Mission 01 local (`01dcb63e8306442f396f982bee51e3da1c98ee80966343f68f94017adb81df38`),
les invariants sont :

| niveau | nombre |
| --- | ---: |
| `Set` / unités | 230 |
| `Act` | 492 |
| `Order` | 2 975 |

Répartition des tags `OrderBin` : `0=233`, `1=206`, `2=890`, `3=442`,
`5=686`, `6=232`, `7=53`, `8=233`; les tags `4` et `9` sont absents.
Le numéro 30 de l’activation (`0x8225A918`, slot virtuel `+0x40`) est une
commande de l’interpréteur de mission et ne doit pas être confondu avec ces
tags `OrderBin` : sa présence et son scheduling dans M01 restent à qualifier
dynamiquement.

## Capture oracle

Une sortie contrôlée fraîche avec le probe `0x8226FEC0` a exécuté les touches
jusqu’aux captures étiquetées « flight-* », mais l’écran est resté au menu de
sélection d’arme : aucune frontière de vol n’est qualifiée. Elle n’a atteint ni
`0x822A6710`/`0x8226D1C8`, ni les 3 600 ticks. Le harness s’est arrêté sur le
prédicat de fin de trace après 99/100 étapes ; `fatal_matches=[]`, aucun
processus `ac6recomp`/`Xvfb` ne reste, et aucun JSONL d’insertion n’a été créé.
La route est donc un essai non qualifié, pas une preuve d’absence d’activation.

## Validation

- build des trois tests scénario/session réussi ;
- CTest ciblé : `2/2` passés ;
- tests Python : `127` passés ;
- `git diff --check` sur les fichiers du checkpoint : propre.

Le gate dynamique `retail_units_and_waves` reste ouvert. Le natif expose
toujours séparément `constructed=230`, `registered=230`, `registry_active=230`,
`combat_active=230`, `placed=95`, `unplaced=135`; cette égalité d’activité est
un comportement du port actuel, pas une preuve de parité retail.
