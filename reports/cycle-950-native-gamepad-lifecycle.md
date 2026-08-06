# Cycle 950 — cycle de vie gamepad SDL natif

La plateforme native ouvre les gamepads déjà présents à l’initialisation,
ouvre ceux des événements `GAMEPAD_ADDED`, puis ferme le handle correspondant
sur `GAMEPAD_REMOVED`. Le retrait continue également de purger les touches,
axes et boutons maintenus.

Validation : CTest normal 3/3 sous Xvfb/audio dummy. Ce chemin est local SDL3
et ne dépend d’aucun transport VNC ; il prépare l’usage d’une manette physique
sur la machine native.
