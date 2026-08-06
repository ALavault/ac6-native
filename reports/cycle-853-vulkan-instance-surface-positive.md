# Cycle 853 — instance et surface Vulkan positives

Date: 2026-08-04

## Résultat

`VulkanInstance` crée/détruit une instance Vulkan 1.0 avec liste d'extensions
SDL validée (noms non vides, sans doublons). Un smoke hors CTest
`ac6-vulkan-surface-smoke` initialise SDL vidéo, charge le loader Vulkan,
récupère les extensions, crée une fenêtre Vulkan sous Xvfb, puis crée et
détruit une `VkSurfaceKHR` réelle.

Sortie du smoke sous Xvfb :

```text
sdl_vulkan_surface=1 extensions=2
```

## Validation

```text
cmake -S reconstruction/ace-combat-6 -B reconstruction/ace-combat-6/build OK
cmake --build reconstruction/ace-combat-6/build -j2                       OK
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-vulkan-surface-smoke             exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

Le chemin dummy refuse naturellement la surface Vulkan; il ne sert pas de
preuve positive.

## Limite

La sélection du physical device/queue, la swapchain, les image views et la
présentation d'un `WorldFrame` restent à implémenter.
