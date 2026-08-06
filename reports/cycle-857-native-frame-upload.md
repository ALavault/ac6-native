# Cycle 857 — upload framebuffer natif vers Vulkan

Date: 2026-08-04

## Résultat

`NativeRenderTarget::copy_rgba8` expose une copie RGBA8 bornée et indépendante
du stockage interne. `VulkanFramePresenter::present_frame` redimensionne par
nearest-neighbor vers l'extent de swapchain, crée un staging buffer host-visible,
copie les pixels, transitionne l'image acquise, exécute
`vkCmdCopyBufferToImage`, soumet et présente.

Le smoke Xvfb présente d'abord un clear Vulkan puis une seconde frame issue
d'une `NativeRenderTarget` 64×32 (clear couleur), avec code 0. Le chemin est
donc framebuffer natif → staging → swapchain → present, mais la cible reste une
fixture et non les assets Mission 01.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-vulkan-surface-smoke        exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

`ac6-native` n'appelle pas encore le presenter; il reste à connecter le
`WorldFrame`, les manifestes retail et la boucle SDL interactive pour obtenir
la première frame Mission 01.
