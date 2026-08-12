# Checkpoint 4 — soumission de scène Vulkan bornée

Date : 2026-08-12

## Résultat

La frontière de transport GPU est maintenant testable indépendamment du
raster CPU. `VulkanSceneRenderer` consomme un `RenderScene`, résout les
identifiants stables de mesh et de matériau vers des ressources Vulkan
persistantes, efface la cible puis appelle `draw_indexed` directement. Le
readback d'un triangle headless confirme une image produite par Vulkan ; le
chemin ne référence pas `NativeRenderTarget` et n'alloue pas de staging par
frame.

Le périmètre accepté pour ce checkpoint est volontairement strict : une passe,
RGBA8 sans profondeur ni MSAA, triangle-list, transformée identité, aucun HUD
ni texture. Une topologie, transformée, texture, passe ou état non supporté
retourne `false` explicitement ; aucun fallback CPU interactif n'est utilisé.

## Validation

```text
vulkan_scene_renderer=pass direct_gpu_draw=1 cpu_target=0
```

```text
ctest -R 'ac6-vulkan-scene-renderer|ac6-vulkan-backend|ac6-render-scene-contract-tests|ac6-vulkan-backend-recovery-manifest|ac6-product-boundary-binary'
100% tests passed, 0 tests failed out of 5
```

Le test vérifie également les refus de topologie non supportée, de translation
non identité et de HUD. `git diff --check` passe sur les fichiers du
checkpoint.

## Limites restantes

Ce n'est pas encore le renderer retail de Mission 01 : la voie de production
`VulkanRenderer` reste CPU pour la compatibilité actuelle, et cette soumission
ne couvre pas les textures, les transforms vivantes, les états de profondeur,
les passes intermédiaires, le swapchain ni la traduction ucode Xenos vers
SPIR-V. Ces ouvertures doivent être fermées par des traces Mission 01 et des
readbacks qualifiés avant de retirer l'oracle raster.
