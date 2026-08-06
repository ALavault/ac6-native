# Cycle 878 — pause du scheduler mission

`MissionExecution::dispatch` expose maintenant les événements HSM au produit.
Quand le scénario est en `Paused`, `MissionRuntime::tick` ne consomme ni
fixed-step ni entrée : tick, positions et transforms restent inchangés.
Après `Resume`, le tick suivant reprend normalement et republie une frame
prête. Le comportement est couvert par un test déterministe.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.
