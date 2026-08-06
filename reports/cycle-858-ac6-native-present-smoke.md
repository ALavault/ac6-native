# Cycle 858 — raccord presenter dans `ac6-native`

Date: 2026-08-04

## Résultat

`ac6-native --present-smoke` initialise SDL vidéo/Vulkan, crée instance,
fenêtre, surface, device, swapchain et presenter, exécute un tick du runtime,
copie une cible native clearée 320×180 vers la fenêtre puis détruit toutes les
ressources dans l'ordre inverse. Le mode développeur est explicite et ne sert
pas de preuve du parcours frontend retail; le démarrage sans argument reste
headless et retourne proprement.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ac6-native                                                        exit 0
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-native --present-smoke    exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

La cible affichée est encore une clear native; `ac6-native` ne charge pas
encore les manifestes Mission 01, ne configure pas `MissionRuntime` avec
scenario/unités/renderer et ne présente donc pas la géométrie retail.
