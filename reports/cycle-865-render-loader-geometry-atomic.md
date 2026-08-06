# Cycle 865 — loader render + géométrie atomique

Date: 2026-08-04

## Résultat

Une surcharge de `MissionManifestLoader::load_render` charge désormais les
dix bases render et vérifie/décode automatiquement chaque buffer référencé
dans `NativeGeometryDatabase`. Les bases et la géométrie sont construites dans
des temporaires puis publiées ensemble; une vérification de hash, un buffer
absent ou un décodage invalide ne laisse pas de renderer partiel.

`ac6-native --present-manifest` utilise cette surcharge et ne duplique plus la
logique de vérification des buffers.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-native --present-smoke    exit 0
```

## Limite

La preuve positive avec un manifeste render qualifié et buffers retail reste à
exécuter localement; le dépôt ne fabrique pas de bytes retail pour la simuler.
