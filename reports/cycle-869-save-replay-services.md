# Cycle 869 — services sauvegarde/replay reliés à la mission

`MissionExecution` expose désormais `snapshot()` et `restore()`. Le mode
`ac6-native --services-smoke <manifest> <mission_id> <save> <replay>` charge
les définitions externes, exécute 30 ticks, écrit puis relit un fichier
`SaveStore` et un `ReplayLog`, rejoue les entrées et vérifie le tick et les
positions finales déterministes. Le refus d'un manifeste absent est testé
(code 28).

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK. Le
format de snapshot actuel couvre tick et position ; les états angulaires
restent un contrat à qualifier avant de déclarer une reprise complète retail.
