# Cycle 972 — input mission et invariance de pause

## Défaut fermé

`InputFrame.buttons` atteignait auparavant uniquement la couche de données de
vol ; `MissionExecution` ne consultait pas le mapping HSM. De plus, le tick
combat et les directeurs de vagues/séquence pouvaient progresser alors que la
scène était en pause.

## Contrat

```text
InputFrame.buttons
  -> InputMappingDatabase::resolve
  -> MissionExecution::dispatch
  -> Pause/Resume HSM
```

Le dispatch intervient avant la simulation. En pause, aucun tick combat,
radio, vague ou séquence n’est exécuté ; la reprise réactive le scheduler au
tick suivant. Les appels sans mapping restent compatibles avec les fixtures
existantes.

## Validation

Le test lance une mission, envoie `Pause` avec pitch/throttle extrêmes et
vérifie que le tick et la pose restent constants, puis envoie `Resume` et
vérifie la reprise au tick suivant.

```text
cmake --build build -j2
DISPLAY=:102 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:102 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

La qualification reste native ; elle ne remplace pas une capture retail
interactive.
