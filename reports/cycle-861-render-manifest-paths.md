# Cycle 861 — couverture chemins render

Date: 2026-08-04

## Résultat

`MissionManifestPaths` accepte désormais les chemins externes de toute la
chaîne render : render definitions, drawables, transforms, materials,
textures, shaders, targets, passes, resolves et buffers. `render_valid()` ne
retourne vrai que si le triplet runtime et ces dix entrées sont présents.

Le manifeste minimal catalog/assets/launches reste valide pour les services
non graphiques. Une fixture ajoute les dix clés, vérifie `render_valid()` et
rejette implicitement toute clé inconnue ou dupliquée via `load_paths`.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Les dix fichiers ne sont pas encore chargés automatiquement dans les bases
correspondantes et `ac6-native` ne consomme toujours pas ce manifeste pour une
frame Mission 01.
