# Cycle 729 — couverture monde sous la pose de vol

Date : 2026-08-04  
Périmètre : vérifier que le changement de pose induit par les axes ne reste
pas seulement numérique, en reprojetant et soumettant le batch géométrique
Scene complet sous la caméra de vol.

## Changement

La fixture SDL conserve maintenant tous les meshes géométriques monde après
application des transforms `Rigid/AnimRigid`. Elle reprojette ce batch complet
avec la caméra produite par `apply_campaign_flight_to_camera()` et le soumet
au pipeline solide générique dans la cible de vol, en plus du mesh texturé
diagnostique. Aucun LOD, force flag ou branche Mission 1 n'est ajouté.

## Preuve non-dummy

```text
DISPLAY=:99 SDL_VIDEODRIVER=x11 Xvfb 640x480x24
vulkan_campaign_sdl_presented=1 mission=1 hud=1
scene_changed=4439 hud_green=4439 world_changed=11 textured_changed=1
scene_draw_groups=3 clip_x=-0.612867:-0.336477
clip_y=0.228415:0.472099 flight_changed=1 flight_world_pixels=12
flight_pixels=4440 mission1_completed=1 mission2_restored=1
mission2_presented=1 mission2_changed=6974 mission2_hud_green=4428
```

`flight_changed=1` et `flight_world_pixels=12` ferment la preuve minimale
axes → pose → projection → rasterisation monde hors HUD. Le résultat ne
qualifie pas encore les textures retail : la batch texturée seule reste à
`textured_changed=1`, et le fallback solide sert uniquement à isoler la
couverture géométrique.

## Validation

```text
build : cmake --build .../reconstruction-material -j2
targeted CTest : 5/5, 1 skip contrôlé sous SDL_VIDEODRIVER=dummy, 1.67 s
full CTest PAL : 63/63, 1 skip contrôlé sous SDL_VIDEODRIVER=dummy, 62.55 s
```

Mission 1 reste terminée par le dispatch générique d'événements, la sauvegarde
`AC6S` est relue depuis disque et Mission 2 est rendue par le même pipeline.
Mission 2 n'est pas encore volée ni complétée; la boucle manette physique,
les matériaux/profondeur et les avions blancs des cutscenes restent ouverts.

## Hashes

```text
tests/campaign_vulkan_sdl_present_tests.cpp   683ae136110a35756e76aceff2b32dc2c0085192bff03360deb77ed2dec68677
include/ac6/campaign_runtime.h                1eec85c10a60a7259af890ed08dc4ec32ab4de1c1cd17b9fa0ee7c5e2eea3561
src/campaign_runtime.cpp                      df1e1b249a756a9916e4b5ffc44b157a04c773dbb2a87ec597159e0a1c5851dd
tests/campaign_runtime_tests.cpp              4eac5fe15ed3aa53f67c43d23e4c337821ec8374f6aa49d84bc4e96fa7aed624
```
