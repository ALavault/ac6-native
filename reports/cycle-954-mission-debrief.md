# Cycle 954 — débriefing MissionExecution

`MissionScenario::debrief()` et `MissionExecution::debrief()` exposent une
vue native stable de la sortie de mission : identifiant, résultat
`InProgress`/`Success`/`Failure`, compte total d’objectifs, objectifs réussis,
objectifs échoués et historique radio.

Le test runtime vérifie les vues succès et échec après propagation vers
`CampaignProgression`. Aucun résultat n’est déduit d’une absence de crash et
aucune branche Mission 1 n’est ajoutée.

Validation : build CMake, CTest (`4/4`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, et smoke SDL3/Vulkan (`swapchain_images=3`).
