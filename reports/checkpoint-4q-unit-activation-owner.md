# Checkpoint 4q — propriétaire de l’activation des unités

Date : 2026-08-12.

## Frontière statique qualifiée

Dans le XEX PAL canonique (`default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`) :

- `0x8226FEC0` écrit l’objet dans la table et incrémente le compte. Son seul
  appel `bl` est dans `0x820A7650`, à l’intérieur du constructeur par-record
  `0x820A7070`, hors boucle des enfants.
- `0x8226FF60` parcourt ensuite les entrées publiées et appelle les slots
  virtuels de l’unité ; ce n’est pas une insertion.
- `0x822980C8` installe l’état FSM initial seulement si `[unit+0xDC] > 0`.
  Le constructeur met ce compteur à zéro.
- L’invocation identifiée de l’activation FSM est `0x8225A918`, commande 30
  de l’interpréteur `0x8225A600`, vers le slot virtuel `+0x40` (`0x822982C0`).
  La présence de cette commande dans la séquence Mission 01 et son scheduling
  restent ouverts ; aucun spawn/déspawn ne doit être inventé avant cette preuve.

Conclusion : `230` est solidement le nombre construit/enregistré au chargement.
Le propriétaire général de l’activité est le couple mission-script/FSM, pas
`0x8226FEC0` lui-même.

## Contrat natif explicite

`RetailPopulationSnapshot` expose séparément :
`constructed`, `registered`, `registry_active`, `combat_active`, `placed` et
`unplaced`. `RetailWorld::population_snapshot()` ne change aucun comportement
mais rend impossible de faire passer l’égalité `230 → 230` pour une preuve de
parité retail. Le test payload Mission 01 épingle
`230/230/230/230/95/135` et l’invariant de construction.

## Validation

- Build cible réussi.
- Tests ciblés `4/4` : session replay, mission state, playable et session.
- Le probe oracle `0x8226FEC0` reste capture-only et sa route n’a pas atteint
  M01 ; aucune gate dynamique n’est fermée.
