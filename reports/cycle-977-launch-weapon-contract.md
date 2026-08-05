# Cycle 977 — templates d’armes dans le lancement mission

## Défaut fermé

Les contrats `CombatWorld` savaient verrouiller, tirer et appliquer des dégâts,
mais un lancement normal ne publiait aucune arme. `MissionExecution::fire_weapon`
ne pouvait donc pas fonctionner à partir d’un `MissionLaunchDefinition`.

## Correction

Le launch contract contient une liste de `WeaponDefinition`. Le chargeur
valide chaque définition et l’unicité des IDs ; l’exécution publie les armes
atomiquement avec les unités et refuse le lancement si une arme est invalide.
Les anciennes définitions sans arme restent valides pour les fixtures.

## Validation

Le test lance deux unités de factions différentes avec une arme, verrouille la
cible, tire, avance quatre pas de `0.25 s` et vérifie les dégâts de collision.

```text
cmake --build build -j2
DISPLAY=:108 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:108 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_manifest=pass qualified=1
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Les paramètres d’arme restent des contrats natifs jusqu’à qualification par
les templates retail.
