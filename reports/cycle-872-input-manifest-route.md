# Cycle 872 — mapping input externe

`MissionManifestPaths` reconnaît désormais la clé `input`, et
`MissionManifestLoader::load_input` charge atomiquement une
`InputMappingDatabase`. `--frontend-smoke` refuse maintenant l'absence de
cette table et utilise le mapping externe pour piloter les transitions, sans
masque bouton codé en dur. Le test couvre résolution et événement
`start_mission`.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.
