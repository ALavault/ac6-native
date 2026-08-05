# Cycle 992 — couverture multi-missions du loader runtime

Le fixture de `MissionManifestLoader::load_runtime` couvre maintenant trois
missions publiées ensemble: `air_intercept` (mission 1), `strike` (mission 2)
et `escort` (mission 3). Chaque route possède des assets et un lancement
distincts; les assertions vérifient les trois familles, les trois assets et
les trois définitions de lancement après publication.

Le test conserve également la validation atomique d'une erreur dans un fichier
optionnel: après l'échec, les trois routes restent présentes. Cela ferme le
garde-fou de chargement demandé pour empêcher une hypothèse codée en dur sur
Mission 1, sans promouvoir les missions retail 2–15 au-delà de leur niveau de
preuve catalogue.

Validation prévue pour ce cycle:

```text
cmake --build build -j2
SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build --output-on-failure
```
