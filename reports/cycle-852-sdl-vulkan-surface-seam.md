# Cycle 852 — seam surface Vulkan SDL3

Date: 2026-08-04

## Résultat

`SdlVulkanSurface` encapsule les extensions d'instance SDL3, la création d'une
`VkSurfaceKHR` contre une fenêtre Vulkan et sa destruction avant le handle de
fenêtre. Les handles Vulkan restent hors du cœur produit.

La création refuse fenêtre invalide, instance nulle et double création. Le
harnais dummy vérifie ces rejets; il ne fabrique pas de surface, car le driver
dummy n'en fournit pas.

## Validation

```text
cmake -S reconstruction/ace-combat-6 -B reconstruction/ace-combat-6/build OK
cmake --build reconstruction/ace-combat-6/build -j2                       OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Il manque encore la création de `VkInstance` avec les extensions SDL, la
surface positive sous X11/Vulkan, la sélection GPU/queue et la swapchain. Ce
cycle ferme seulement le seam de propriété des handles.
