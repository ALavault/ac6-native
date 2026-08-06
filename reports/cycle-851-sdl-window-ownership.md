# Cycle 851 — ownership fenêtre SDL3

Date: 2026-08-04

## Résultat

`SdlWindow` possède la création, l'affichage et la destruction d'une fenêtre
SDL3. Le drapeau `SDL_WINDOW_VULKAN` est sélectionnable par l'appelant; les
tests headless utilisent une fenêtre cachée non-Vulkan, car le driver dummy ne
fournit pas de surface Vulkan.

La fenêtre est détruite avant la fermeture du sous-système events. Les handles
restent opaques au cœur et aucun shell de diagnostic n'est ajouté.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

La fenêtre n'est pas encore créée par `ac6-native`, et aucun `SDL_Vulkan`
surface/swapchain n'est encore branché. La preuve headless ne vaut donc pas
capture visible.
