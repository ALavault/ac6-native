# Cycle 855 — swapchain Vulkan

Date: 2026-08-04

## Résultat

`VulkanSwapchain` interroge les capacités, formats et modes de présentation
de la surface, exige le mode FIFO, choisit un format couleur valide, borne
l'extent demandé, crée la swapchain et toutes ses image views. Toute erreur
partielle détruit les views et la swapchain déjà créées.

Le smoke Xvfb enchaîne désormais instance → surface → physical device/queue →
swapchain et termine avec code 0. Le nombre d'images est observé dans sa sortie
stderr; le chemin dummy reste limité au CTest cœur.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-vulkan-surface-smoke        exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Il manque encore render pass/framebuffers, command pool/buffers, acquisition,
submit/present et le transfert du framebuffer natif vers une image Vulkan.
