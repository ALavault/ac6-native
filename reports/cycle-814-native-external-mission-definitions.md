# Cycle 814 - definitions de missions externes

Le catalogue natif accepte maintenant un manifeste TSV strict :
`mission_id<TAB>family<TAB>asset_ids`. Les familles retenues sont
`air_intercept`, `strike` et `escort`; les assets sont des IDs natifs stables
separes par virgules. Les lignes invalides, familles inconnues, assets nuls,
assets dupliques et missions dupliquees echouent fail-closed.

`FrontendController::select_mission` refuse toute mission absente du catalogue.
`MissionRuntime::set_definition` refuse une definition nulle, inconnue ou dont
l'ID ne correspond pas au runtime courant. La readiness n'utilise plus
`mission_id == 1` et parcourt uniquement les `asset_ids` de la definition
chargee.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`

Les deux commandes passent; CTest reste a `1/1`.
