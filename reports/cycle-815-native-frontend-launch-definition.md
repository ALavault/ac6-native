# Cycle 815 - definition de lancement frontend

`FrontendController::mission_definition` retourne une definition seulement si
le frontend a atteint `FrontendState::Mission` et si la mission selectionnee
existe encore dans le catalogue fourni. Le runtime dispose aussi d'un
constructeur `MissionRuntime(const MissionDefinition&, assets)`, qui initialise
son `mission_id` et sa gate d'assets depuis les donnees catalogue.

Cette route couvre le raccord minimal :
Title -> New Game -> Briefing -> Hangar -> Loading -> Mission -> definition
catalogue -> runtime natif.

Une regression de test a ete corrigee pendant ce cycle : `assert` etait utilise
pour executer `advance()`, mais le build principal est Release avec `-DNDEBUG`.
Le test emploie maintenant `REQUIRE`, actif en Release, afin que les controles
et leurs effets de bord restent executes.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- `ctest --test-dir reconstruction/ace-combat-6/build-asan --output-on-failure`

Les trois validations passent; CTest reste a `1/1` dans les deux builds.
