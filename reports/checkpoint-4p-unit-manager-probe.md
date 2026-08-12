# Checkpoint 4p — probe d’insertion du `CX360UnitManager`

Date : 2026-08-12.

## Résultat

Le checkout oracle capture possède maintenant un hook capture-only sur
`0x8226FEC0`. Il écrit, dans un JSONL borné et séparé de
`ac6.execution-trace.v2`, le compteur avant/après, l’index d’insertion,
l’adresse de l’objet, `record_index`, les flags et `object_count`. Le code
généré n’a pas été modifié et le probe n’est pas lié au produit.

Le harness accepte `--unit-boundary-output` et conserve ce chemin dans la
commande oracle. Le checkout capture recompilé passe l’édition de liens et
expose bien les symboles forts `rex_sub_8226FEC0`, `rex_sub_8226D1C8` et
`rex_sub_822A6710`.

## Capture non qualifiée

La route privée avec `SDL_AUDIODRIVER=dummy` a atteint la transition de
campagne, mais pas l’entrée Mission 01 : la route courte s’est arrêtée sur
timeout après 4/97 étapes et la route complète après 61/88 étapes. Aucun
`unit-manager-insertion.jsonl` n’a donc été produit. Il n’y a ni capture
dynamique ni fermeture du gate `retail_units_and_waves`.

## Invariant maintenu

Le natif continue de distinguer les mesures : `RetailWorld::published` vaut
230 pour les objets construits depuis le payload, tandis que le premier
`WorldFrame::active_units` vaut aussi 230 parce que `RetailSession` installe
actuellement chaque état de combat comme actif. Cette égalité est un résultat
du port partiel ; elle ne qualifie pas l’activité retail. Les 95 positions
résolues et 135 absentes restent une partition de placement, jamais une
preuve de despawn.

## Prochaine preuve recevable

Reprendre la route oracle jusqu’aux frontières `0x822A6710` puis
`0x8226D1C8`, armer le probe après l’entrée en vol et vérifier si les
insertions `0x8226FEC0` sont limitées au chargement ou réapparaissent pendant
les 3 600 ticks. Ensuite seulement relier une publication/retraction à un
`Set → Act → Order` ou à un consommateur du gestionnaire de mission.

Validation : build oracle capture réussi ; tests Python ciblés `37 passed,
8 subtests`; build natif et CTest `81/81`, avec les trois skips
environnementaux connus.
