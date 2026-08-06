# Cycle 876 — exécution multi-familles

Le test déterministe lance désormais une mission `AirIntercept` puis une
mission `Strike` avec des définitions, assets et unités différents. Les deux
publient un `WorldFrame` prêt avec ownership joueur correct via le même
`MissionExecution`; aucune branche spécifique au numéro de mission n’est
nécessaire.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.
