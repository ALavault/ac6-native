# Cycle 800 — gate d’assets Mission 01

`MissionRuntime` accepte désormais une base d’assets externe et ne marque
Mission 01 prête que lorsque les IDs qualifiés 9, 119, 165, 199 et 210 sont
résolus. Un manifeste incomplet reste fail-closed ; le test couvre d’abord
l’échec puis la réussite après ajout des quatre entrées manquantes.

Validation : build CMake, CTest `1/1`, et démarrage développeur sans manifeste
terminant par le code 2 attendu.
