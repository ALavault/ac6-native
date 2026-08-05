# Cycle 996 — intégration CLI du bundle services

Les commandes natives `--validate-manifest`, `--frontend-smoke`,
`--services-smoke` et `--present-manifest` utilisent maintenant la surcharge
`MissionManifestLoader::load_runtime(..., MissionRuntimeServices&)`.
`frontend-smoke` consomme le mapping input publié; `services-smoke` et
`present-manifest` transmettent les bases objectifs/radio à
`MissionExecution`; `validate-manifest` vérifie la présence du bundle campagne
quand le manifeste la déclare.

La compatibilité de l'ancienne surcharge est conservée pour les inspecteurs et
appelants limités au trio catalogue/assets/lancements. Aucun chemin CLI ne
réintroduit de données retail ou de branche Mission 1.

Validation:

```text
cmake --build build -j2                         pass
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...     5/5 pass
SDL_AUDIODRIVER=dummy xvfb-run -a ./build/ac6-native --present-smoke  pass
```
