# Cycle 813 — catalogue déclaratif de familles

Ajout de `MissionCatalog` et `MissionDefinition` : chaque mission déclare une
famille de comportement et ses IDs d’assets. Les définitions invalides,
familles inconnues et doublons sont rejetés ; aucune branche runtime par
`mission_id` n’est introduite.

Validation : build CMake et CTest `1/1` réussi.
