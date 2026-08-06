# Cycle 979 — directeur IA déterministe

## Contrat

Le runtime possédait des vagues et des armes, mais aucun raccord générique
entre une unité contrôlée par l’IA, sa cible et son arme. `MissionAiDirector`
introduit des règles bornées :

```text
mission_id, first_tick, period_ticks, entity, target, weapon_id
```

Une règle active verrouille sa cible puis utilise `CombatWorld::fire`. Les
tentatives sont déterministes et les cooldowns restent ceux du combat partagé.

## Preuve

Le test rejette une règle dupliquée, lance deux unités de factions distinctes,
applique une règle au tick 1 et vérifie projectile puis dégâts de la cible.

```text
cmake --build build -j2
DISPLAY=:110 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:110 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_manifest=pass qualified=1
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Ce directeur est un contrat natif générique ; les comportements IA et
paramètres retail restent à qualifier par mission.
