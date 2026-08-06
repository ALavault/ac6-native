# Cycle 718 — surface SDL → instance Vulkan → swapchain native

Date : 2026-08-03  
Périmètre : enlever la dépendance headless du chemin de présentation sans
introduire SDL dans le backend AC6.

## Résultat

`VulkanCampaignBackend::create_with_surface` crée l'instance Vulkan, appelle
une `VulkanSurfaceCreateCallback` avec son handle opaque, sélectionne une
queue graphique présentable sur cette surface et conserve la surface jusqu'à
`create_presentation_target`. Le chemin de copie/acquire/present est ensuite
commun au headless et aux fenêtres de plateforme. La surface pending est
libérée si la création du device, du target ou du backend échoue.

Le test SDL3 `ac6-vulkan-sdl-window-tests` récupère les extensions de la
fenêtre, crée la surface avec `SDL_Vulkan_CreateSurface` et tente le même
target/pipeline/triangle/present. Sous `SDL_VIDEODRIVER=dummy`, la création
de fenêtre Vulkan est indisponible et le test donne un skip explicite
`vulkan_sdl_presentation_skipped=window`; il ne transforme pas ce cas en
présentation observée. Sur un driver SDL/Vulkan réel, le même binaire devient
le checkpoint de présentation sans changement du renderer.

Le test backend exerce aussi la callback avec une surface headless et vérifie
la fermeture contrôlée sur le driver local (`surface_or_swapchain`).

## Validation

```text
ac6-vulkan-backend-tests        : pass
ac6-vulkan-sdl-window-tests     : pass (skip contrôlé sous dummy)
CTest avec AC6_ASSET_ROOT PAL  : 56/56 pass, 69.15 s
```

La dernière garde `headless_surface_extension_enabled` a ensuite été
recompilée avec les deux tests ciblés (2/2 pass). La présentation réelle reste
non observée dans cet environnement faute de surface/swapchain fonctionnelle.

## Hashes

```text
include/ac6/vulkan_backend.h        ea77d3fa98fde09871a828c6871a66716bae0a19fcc9e6eae054e5284717e38c
src/vulkan_backend.cpp               1a4a709027c09db03a205c77904b304ab044e881a0a903f9cf7597fcb0e2cdd6
tests/vulkan_backend_tests.cpp       006bb6e76018c0b5d9a0f81fa983e7d80f8e7df4709a0d211ebe8f040f771ace
tests/vulkan_sdl_window_tests.cpp    df2dddf462f0544b4bd145247c5bf27943a82626cc7b2a40a1c321711db8d0de
CMakeLists.txt                       cde06ac4664028113cf052f149bc43060bf005b846e973939d8c07b9b4c29b46
```

## Frontière restante

Le shell SDL historique rend encore sa scène avec son renderer SDL; ce test
ne prétend donc pas avoir remplacé ce chemin ni rendu Mission 1. Il ferme
seulement le contrat de présentation Vulkan indépendant de SDL. Il faut
maintenant faire alimenter cette cible par la frame campagne et ses meshes
réels, puis qualifier le monde noir, le HUD retail, la sauvegarde et le
déverrouillage Mission 2.
