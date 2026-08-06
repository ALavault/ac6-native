# Cycle 816 - manifeste de lancement natif

Ajout de `MissionLaunchDefinition` et `MissionLaunchDatabase`. Le format TSV
strict est :
`mission_id<TAB>player_entity<TAB>unit_id:owner:asset,...`.

Les definitions invalides sont rejetees : mission nulle, joueur nul, liste
d'unites vide, unite nulle, asset nul, auto-ownership, unite dupliquee ou
joueur absent de la liste. `configure_mission_launch` refuse aussi un lancement
dont `mission_id` ne correspond pas au `MissionScenario` courant.

Le test produit couvre maintenant la route :
catalogue frontend -> `MissionDefinition` -> `MissionLaunchDefinition` ->
`UnitRegistry`/`MissionScenario` -> `MissionRuntime::tick`.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur Xbox/oracle;
`ldd` ne liste que les dependances Linux standard.
