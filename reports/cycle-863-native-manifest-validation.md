# Cycle 863 — validation manifeste dans ac6-native

Date: 2026-08-04

## Résultat

`ac6-native --validate-manifest <path>` consomme désormais le manifeste
externe, exige `render_valid()`, charge catalog/assets/launches puis les dix
bases render via `MissionManifestLoader`. Un chemin absent, une couverture
incomplète ou une base invalide retourne un code d'échec distinct; aucune
session interactive n'est démarrée.

Le mode est explicitement développeur et séparé de la preuve frontend. Aucun
asset ou chemin retail n'est embarqué dans le binaire.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ac6-native --validate-manifest /tmp/no-such-ac6-manifest            exit 6
ac6-native                                                        exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Le mode valide les bases mais ne lance pas encore `MissionExecution`, ne
instancie pas le renderer à partir des bases et ne présente pas la géométrie
Mission 01.
