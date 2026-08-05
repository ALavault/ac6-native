# Cycle 962 — directeur de vagues d’unités

`MissionWaveDirector` décrit des apparitions génériques par mission et tick.
Les entrées sont triées par mission, tick et entité; une vague due est publiée
atomiquement dans `UnitRegistry` et `CombatWorld`, et un échec laisse les deux
registres inchangés. Le despawn utilise la même frontière atomique.

`MissionExecution` appelle le directeur après chaque tick et met à jour le
compteur d’unités actif. Le test couvre apparition différée, doublon
d’identifiant, compteur pending/spawned et despawn.

Build, CTest (`5/5`) sous Xvfb avec `SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan
et audit campagne passent. Les paramètres de vagues retail restent à charger
depuis un manifeste qualifié; aucune identité de mission n’est extrapolée.
