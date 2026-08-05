# Cycle 975 — replay au niveau MissionExecution

## Contrat

Le `ReplayLog` persistait déjà les axes, throttle et boutons, mais la boucle
mission n’exposait pas de relecture unifiée. `MissionExecution::run_replay`
réutilise `tick` pour chaque frame : les commandes HSM, la pause, la reprise,
le combat et la progression suivent donc exactement le chemin interactif.

## Preuve

Le test lance deux exécutions identiques avec un replay neutre → pause →
reprise, puis compare tick, pose et état `Gameplay` final. Toute divergence
serait visible dans le même test A/B.

```text
cmake --build build -j2
DISPLAY=:105 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:105 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Le replay est un contrat natif déterministe ; il ne constitue pas une preuve
de parité retail tant que le replay retail de référence n’est pas qualifié.
