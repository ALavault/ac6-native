# Cycle 877 — loaders runtime atomiques

`MissionCatalog`, `MissionAssetDatabase` et `MissionLaunchDatabase` chargent
désormais dans une instance temporaire et ne publient qu'après validation de
toutes les lignes. Des tests injectent une seconde ligne invalide et vérifient
que l'entrée valide précédente reste intacte, sans entrée partielle.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.
