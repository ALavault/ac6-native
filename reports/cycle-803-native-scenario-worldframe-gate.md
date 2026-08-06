# Cycle 803 — gate scénario → WorldFrame

`MissionRuntime` référence maintenant un `MissionScenario` explicite. Même
avec les cinq assets Mission 01 résolus, `mission_ready` reste faux tant que
le scénario n’est pas en `Gameplay`; l’événement `StartMission` ouvre ensuite
la production prête. Le test couvre les deux états de gate.

Validation : build CMake et CTest `1/1` réussi.
