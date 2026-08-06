# Cycle 854 — physical device et queue Vulkan

Date: 2026-08-04

## Résultat

`VulkanDevice` énumère les physical devices, exige l'extension
`VK_KHR_swapchain`, puis choisit une famille de queue à la fois graphique et
présentable sur la surface SDL. Il crée le device logique et expose la queue;
la destruction attend le device avant de libérer ses handles.

Le smoke Xvfb du cycle 853 exerce maintenant cette sélection après la surface
réelle et termine avec succès (code 0). Le CTest dummy reste séparé et ne
prétend pas fournir une surface Vulkan.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-vulkan-surface-smoke        exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

La swapchain, les images/views, command pool/buffers et la présentation du
framebuffer natif restent à implémenter.
