# Cycle 721 — CUT/TCAM réel vers projection Vulkan

Date : 2026-08-03  
Périmètre : remplacer le cadrage AABB de la preuve renderer par la caméra
réellement référencée par le CUT de l’entrée 9, sans modifier la sélection de
mission ni inventer un acteur.

## Implémentation

`campaign_scene_frame` rejoue un groupe `Scene` décodé, vérifie le contrat
`CutStart → FrameStart(1) → MoveCamera`, résout le chemin adjacent via
`Scene/resource-FHM`, échantillonne le `TcamTrackView` et joint les
`Rigid/AnimRigid` du même frame à leurs `MopTransformTrackView`.

`transform_campaign_mesh` réutilise la convention XYZ déjà exercée par le
shell SDL. `CampaignCameraProjectionConfig` expose maintenant un
`camera_forward_sign` explicite. Le corpus PAL observé place le monde du
premier frame en profondeur locale négative ; le test qualifie donc `-1` au
lieu de prendre une profondeur absolue.

La sélection de renderable par matériau reste générique (`first_qualified` par
défaut, variantes d’inspection disponibles) et ne contient aucune branche
Mission 1/2.

## Validation

```text
ac6-campaign-scene-frame-tests                         PASS
ac6-camera-projection-tests                            PASS
ac6-campaign-retail-frame-tests                       PASS
ac6-campaign-vulkan-retail-frame-tests                PASS

selector 1 / entry 9:
  camera = Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop
  world transforms joined = 16
  forward sign = -1
  real mesh/material submitted = true
  scene_changed = 0 at 128x128 (mesh initial sous-pixellaire)
  HUD readback = 342 green pixels

selector 2 / entry 10:
  AABB renderer smoke retained because no Scene/TCAM group exists
  scene_changed = 8568
  HUD readback = 342 green pixels

targeted CTest : 5/5 pass, 1.67 s
full CTest PAL avec `AC6_ASSET_ROOT` : 59/59 pass, 63.14 s
```

`scene_changed=0` pour le premier frame n’est pas présenté comme une image de
monde : la géométrie réelle résolue est trop petite à cette résolution et ce
point de vue. La preuve nouvelle porte sur la chaîne déterministe
`CUT → TCAM → transform → clip → submission`, tandis que la visibilité
graphique reste une frontière ouverte.

## Hashes

```text
include/ac6/campaign_scene_frame.h          376f6904f552a470949a5a436707de358a83b9127ffffe00eed4ac433488071c
src/campaign_scene_frame.cpp                 b9d3df4444158b54be8a7f72dfddbdf95dd3e124ec9a64df07fbc7a4975d319b
include/ac6/camera_projection.h              db45877349cbb9bcdd1233183f7777a53a65a836b2b310c607e9844a62599610
src/camera_projection.cpp                    9b6147abd45053622e3ed3c3913122f94ee70396ceee85b48d4d6a1a89d2d0b6
include/ac6/campaign_retail_frame.h          230a861a9c923a3a232dc0318e63088c0ae96e5352b768e56b54735462221b08
src/campaign_retail_frame.cpp                456712b854946735c9cb278341322cd1a57b2f5ea303d080bca76b7c28577c11
tests/campaign_scene_frame_tests.cpp         4637747decac9590aa9777da36b843b30deaa3cb549c368d4fa0bfa5b1177d86
tests/campaign_vulkan_retail_frame_tests.cpp 1435f9269502a0478ff8001d93ceffff28748db7dfa22a58deffdfc248166c0b
tests/camera_projection_tests.cpp            a87512b4296a7e020bac50b03de503c27b3cdc403f97e20fb9039be1ce8e44ce
CMakeLists.txt                               8784567665fb1e7707b11a984a8c563a31b6fff1b9367d77fd802fbaff5be7b5
```

## Frontière suivante

Le prochain raccord doit alimenter cette frame depuis l’état runtime
`STANDBY → MISSION 1` et soumettre un batch de monde assez large pour une
preuve non noire à résolution normale, sans abandonner le contrat TCAM. La
swapchain réelle, les contrôles en vol, la sauvegarde et Mission 2 restent
non qualifiés.
