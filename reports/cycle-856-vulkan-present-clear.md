# Cycle 856 — premier submit/present Vulkan

Date: 2026-08-04

## Résultat

`VulkanFramePresenter` possède command pool/buffer, sémaphores et fence,
acquiert une image de swapchain, effectue les transitions de layout, clear une
couleur, soumet à la queue puis appelle `vkQueuePresentKHR`. Les ressources
sont attendues et détruites dans l'ordre.

Le smoke Xvfb enchaîne maintenant instance → surface → device → swapchain →
acquisition/clear/submit/present et termine avec code 0. Cette première frame
est une couleur de validation, pas encore le `WorldFrame` Mission 01.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-vulkan-surface-smoke        exit 0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## ETA révisée

Une première frame Vulkan visible de validation est atteinte. Pour une
première frame Mission 01 native : environ 3–6 checkpoints techniques
(transfert framebuffer, boucle `ac6-native`, chargement manifestes/assets),
hors aléas de qualification avion/matériaux retail.

## Limite

Le presenter ne copie pas encore les pixels de `NativeRenderTarget` et
`ac6-native` ne lance pas la boucle interactive.
