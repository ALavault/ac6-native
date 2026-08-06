# Cycle 862 — chargement atomique des bases render

Date: 2026-08-04

## Résultat

`MissionManifestLoader::load_render` charge les dix bases render dans des
instances temporaires : définition render, drawables, transforms, materials,
textures, shaders, targets, passes, resolves et buffers qualifiés. La
publication dans les bases appelantes n'a lieu qu'après succès des dix
manifests; un chemin absent ou une ligne invalide laisse les sorties intactes.

Le manifeste doit satisfaire `render_valid()` avant tout chargement graphique.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

Les bases individuelles et les guards restent couvertes par la fixture
renderer existante; le raccord positif complet attend un jeu de manifests
render qualifié persistant.

## Limite

`ac6-native` ne consomme pas encore `load_render`; aucune frame retail n'est
claimée par ce cycle.
