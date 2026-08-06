# Cycle 802 — scénario, événements et ownership joueur

Ajout de `MissionScenario` avec états `Loading`, `Gameplay`, `Paused`,
`Complete`, `Aborted`, transitions par `EventType` explicite et registre
`EntityId` du joueur. Le démarrage de mission refuse un sujet différent de
l’entité possédée ; les transitions invalides sont rejetées.

Validation : build CMake et CTest `1/1` réussi, incluant un contrôle positif et
un contrôle négatif d’ownership.
