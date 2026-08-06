# Cycle 715 — frontière de présentation Vulkan headless

Date : 2026-08-03  
Périmètre : préparer la surface/swapchain Vulkan sans dépendance SDL et relier
une cible persistante à une présentation par copie d'image.

## Résultat

`VulkanCampaignBackend::create_with_extensions` accepte désormais les
extensions d'instance et de device explicites. Quand
`VK_EXT_headless_surface` est demandé, la sélection du physical device exige
une queue graphics/compute réellement présentable; cela évite de sélectionner
le GPU NVIDIA non-présentable alors que llvmpipe expose la surface.

Le backend implémente ensuite le chemin surface, swapchain, acquire, copie
`vkCmdCopyImage` depuis une cible persistante et `vkQueuePresentKHR`.

Sur l'environnement courant, la surface headless est créée et le test
sélectionne le device présentable, mais le driver refuse ensuite la création du
swapchain (`VK_ERROR_INITIALIZATION_FAILED`). Le test est donc un skip contrôlé
`vulkan_headless_presentation_skipped=surface_or_swapchain`; aucune
présentation n'est revendiquée comme prouvée.

## Validation

```text
ac6-vulkan-backend-tests : pass (présentation headless skip contrôlé)
CTest avec AC6_ASSET_ROOT : 54/54 pass, 61.46 s
```

La frame persistante, les contrats shader et le readback restent validés par
les mêmes tests. La présentation SDL/native reste la frontière ouverte.

## Hashes

```text
include/ac6/vulkan_backend.h   5043e3f06023998b23ca000b2a2b3161ae2f660af0764c238c55b691ee8df833
src/vulkan_backend.cpp         e6f2e1f183cd8076bc15bbb4ecfdf56dde8afd815f02c33e85a3d16e77f1f12e
tests/vulkan_backend_tests.cpp 11a4778bf64391e01a62bc3b1f344e55d21408da0c4d95a7f31fcc750dfc38f1
```
