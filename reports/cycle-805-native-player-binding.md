# Cycle 805 — liaison joueur/registre

`MissionScenario::bind_player` exige maintenant une unité présente et active
dans `UnitRegistry`. Les unités absentes ou encore inactives ne peuvent pas
devenir le joueur ; le contrôle d’ownership au démarrage reste appliqué.

Validation : build CMake et CTest `1/1` réussi.
