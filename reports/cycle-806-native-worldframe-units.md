# Cycle 806 — unités actives dans WorldFrame

`WorldFrame` expose maintenant le nombre d’unités actives et l’`EntityId` du
joueur. Ces valeurs viennent exclusivement de `UnitRegistry` et
`MissionScenario`; sans registre/scénario elles restent nulles. Le test lie
une unité active au joueur et vérifie la publication correspondante.

Validation : build CMake et CTest `1/1` réussi.
