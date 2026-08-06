# Cycle 730 — capture écran X11 après présentation

Date : 2026-08-04  
Périmètre : vérifier visuellement la présentation SDL/Vulkan, au-delà du
readback de la cible persistante.

## Capture

Le harness accepte désormais `AC6_SCREENSHOT_HOLD_MS` pour maintenir la
fenêtre après `vkQueuePresentKHR`, sans modifier le chemin par défaut. Avec une
pause de 5 secondes, la capture système a été prise après la ligne de sortie
`vulkan_campaign_sdl_presented=1`.

```text
DISPLAY=:99 SDL_VIDEODRIVER=x11
AC6_SCREENSHOT_HOLD_MS=5000
window: AC6 campaign Vulkan frame, 640x360
capture: /home/lavaulta/Pictures/screenshot-2026-08-04_02-59-11.png
```

La PNG est 640×480 et contient exactement 307 200 pixels `(0,0,0)`. Le
readback Vulkan de la même exécution reste positif (`scene_changed=4439`,
`hud_green=4439`, `flight_world_pixels=12`, `mission2_changed=6974`). Cette
capture ferme donc une preuve négative importante : le contenu atteint la
cible Vulkan et la fonction de présentation retourne succès, mais il n'est
pas visible dans la fenêtre X11 sous Xvfb.

## Validation

```text
build : OK
focused CTest : 2/2, 1 skip SDL dummy, 0.04 s
```

Ce n'est pas une régression de la logique de campagne; c'est une nouvelle
frontière de présentation/composition à traiter avant toute affirmation de
parité visuelle. Le fichier est une capture locale, pas un asset retail.

## Hash

```text
tests/campaign_vulkan_sdl_present_tests.cpp
647b11ffac790346242335efbfb00fe47f6d571be570b08b98ddd1f326369560
```
