# Cycle 868 — frontend natif pilotable

`ac6-native` expose maintenant `--frontend-smoke <manifest> <mission_id>`.
Le mode charge le catalogue, les assets et le lancement externes, sélectionne
la mission puis injecte cinq événements `StartMission` via
`InputMappingDatabase`, jusqu'à l'état `Mission`. Toute étape invalide échoue
avec un code distinct ; ce smoke ne revendique pas la preuve visuelle retail.

Validation : build CMake, CTest `1/1`, manifeste absent refusé (code 21), smoke
SDL3/Vulkan sous Xvfb code 0, audit binaire sans symboles Xbox/Xenia/PPC.
