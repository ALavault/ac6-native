# Cycle 708 — contrat caméra/matrices native

Date : 2026-08-03  
Périmètre : transformer un `TcamCameraState` qualifié et un `CampaignMesh`
en vertices clip-space consommables par un backend Vulkan.

## Résultat

Le nouveau contrat `ac6/camera_projection.h` fournit :

- une matrice view row-major à vecteurs-colonnes, dont les trois lignes de
  base reprennent explicitement la convention XYZ déjà utilisée par la
  présentation TCAM bornée ;
- une matrice perspective Vulkan avec profondeur `[0,1]`, `near_plane` et
  `far_plane` explicites ;
- un résultat de projection qui conserve `w`, les UV, les indices,
  `first_texture_id` et `primitive_flags` ;
- des rejets déterministes pour viewport/FOV/plans invalides, caméra ou
  vertices non finis, vertices derrière le plan proche et indices hors
  bornes.

La fixture couvre caméra identité, translation, profondeur perspective,
propagation des métadonnées et les rejets fail-closed. Elle ne prétend pas
résoudre l'ordre de rotation Xenos, le layout des registres Xenos, le
clipping, les `primitive_flags`, les matériaux MATE ou la présentation
swapchain.

## Validation

```text
ac6-camera-projection-tests : pass
ac6-vulkan-backend-tests : pass
ac6-campaign-retail-asset-tests : pass
CTest complet avec AC6_ASSET_ROOT : 54/54 pass, 62.76 s
```

Le test retail conserve la preuve précédente : selector 1 → DATA.TBL[9] et
selector 2 → DATA.TBL[10], lectures PAC bornées uniquement, meshes NDXR réels
préparés côté CPU. Aucune qualification interactive Mission 1/Mission 2 n'est
ajoutée par ce cycle.

## Hashes

```text
include/ac6/camera_projection.h       456dbaac67fb340058285b738900de88fbe5e06d1b1886ebc1fbb00b85aaeb59
src/camera_projection.cpp             ea705a3ec8172167bb240705834f31515a4e26a95029f94d53236d6c1f7bc80e
tests/camera_projection_tests.cpp     df84e118d9f46e8cdba143defcc1321d10d53411c02b96a8a9c8070814733533
CMakeLists.txt                        4dbe091d4ca4f4f513422cc6a671c0e9769b4504446ef2fb33b14259ca5737ae
```

## Frontière suivante

Le prochain seam doit raccorder ces vertices clip-space à un pipeline qui
accepte `w` et une cible depth/presentation, puis qualifier le premier
matériau/texture MATE→NTXR sur une maille retail. Cela reste séparé de toute
affirmation de gameplay ou de sauvegarde Mission 2.
