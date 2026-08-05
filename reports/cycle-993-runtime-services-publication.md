# Cycle 993 — publication atomique des services runtime

`MissionManifestLoader::load_runtime` possède maintenant une surcharge qui
publie `InputMappingDatabase`, `MissionObjectiveDatabase`,
`RadioMessageDatabase` et `CampaignProgression` dans
`MissionRuntimeServices`. Les chemins optionnels sont toujours facultatifs,
mais lorsqu'ils sont présents leur contenu devient disponible au runtime au
lieu d'être simplement validé puis détruit.

Le chargement construit encore toutes les bases dans des objets temporaires et
ne publie le catalogue, les assets, les lancements et les services qu'après la
dernière validation. Le test charge simultanément trois familles de mission,
un objectif, une radio et une route campagne, puis injecte un manifeste input
invalide et vérifie que l'ancien bundle complet reste intact.

Cette interface reste générique : elle ne contient aucun identifiant Mission
01 ni aucune branche spécifique retail. Les services audio/XMA effectifs
restent une frontière remplaçable derrière `RadioMessageDatabase`.

Validation:

```text
cmake --build build -j2                         pass
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...     5/5 pass
```
