# Cycle 994 — intégration du bundle services dans le flux mission

Le test runtime consomme maintenant le bundle publié, au lieu de vérifier
uniquement sa présence. Il fait progresser la campagne chargée par
`MissionRuntimeServices` de `Briefing` à `Active`, fournit le loadout, lance
la définition correspondante avec ses objectifs et sa radio, puis vérifie
objectif complété, état campagne `Completed` et débrief `Success`.

Le même objet services est ensuite soumis à un manifeste input invalide; son
état campagne complété et ses bases déjà publiées restent inchangés. Le flux
est générique et utilise les identifiants du fixture uniquement comme données
de test.

Validation:

```text
cmake --build build -j2                         pass
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...     5/5 pass
```
