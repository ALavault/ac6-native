# Cycle 731 — capture SDL/Vulkan visible après correction

Date : 2026-08-04  
Périmètre : fermer la divergence entre readback Vulkan et écran X11 observée
au cycle 730.

## Correction

Les smoke tests créaient la fenêtre avec `SDL_WINDOW_HIDDEN` et ne l'affichaient
jamais. Le swapchain était donc alimenté mais restait invisible au bureau.
`SDL_ShowWindow()` est maintenant appelé avant la création de la surface dans
les deux tests SDL. Le backend préfère aussi `VK_FORMAT_R8G8B8A8_UNORM`, le
format des cibles natives, avant le fallback BGRA pour toute future copie
directe d'image.

Le harness conserve `AC6_SCREENSHOT_HOLD_MS` (off par défaut) pour maintenir la
fenêtre après la présentation pendant une capture.

## Preuve visuelle

```text
DISPLAY=:99 SDL_VIDEODRIVER=x11
AC6_SCREENSHOT_HOLD_MS=5000
capture: /home/lavaulta/Pictures/screenshot-2026-08-04_03-06-00.png
window: AC6 campaign Vulkan frame, 640x360 au centre du bureau 640x480
```

La capture montre effectivement l'avion/monde au centre et les deux segments
HUD verts. Les bandes noires supérieure et inférieure sont le bureau Xvfb hors
fenêtre; le contenu de la fenêtre n'est plus noir.

La même exécution produit :

```text
scene_changed=4439 hud_green=4439 world_changed=11 textured_changed=1
flight_changed=1 flight_world_pixels=12 flight_pixels=4440
mission1_completed=1 mission2_restored=1 mission2_presented=1
mission2_changed=6974 mission2_hud_green=4428
```

## Validation

```text
build : OK
focused CTest : 4/4, 1 skip SDL dummy, 2.16 s
full CTest PAL : 63/63, 1 skip SDL dummy, 68.30 s
```

La couverture texturée reste partielle (`textured_changed=1`); cette capture
ferme seulement la visibilité de la frame et laisse les contrats matériaux,
profondeur et parité cutscene comme frontière suivante.

## Hashes

```text
src/vulkan_backend.cpp                      30a5df60642a0e044d3f03f7790df85c98b750c043fd1d647f51d9a7cd105642
tests/campaign_vulkan_sdl_present_tests.cpp 2b62c3e3d3a46b7834f92881a89e8e000e2425fa9f90ef727e0be12e27f94b08
tests/vulkan_sdl_window_tests.cpp           cf427b847902b214744b6b5b6f849e185f948c2aaf5a23a07683ed68fba1a36c
```
