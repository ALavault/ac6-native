# Cycle 980 — règle IA et unités inactives

Une unité ciblée par une règle périodique peut être détruite entre deux ticks.
Le directeur IA vérifie l’activité de la source et de la cible avant le lock ;
une entité absente ou inactive est ignorée, tandis qu’un lock invalide sur deux
unités actives reste une erreur explicite.

Validation : après collision et dégâts, le test désactive la cible puis vérifie
que le tick suivant reste `mission_ready`.

```text
DISPLAY=:111 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_manifest=pass qualified=1
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```
