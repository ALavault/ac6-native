# Cycle 709 — raccord clip-space Vulkan et cible depth

Date : 2026-08-03  
Périmètre : exécuter le résultat caméra du cycle 708 dans le backend
AC6-owned, avec `w` explicite et une variante de render target depth.

## Résultat

Le backend expose maintenant :

- `create_clip_mesh_pipeline`, dont le vertex input est
  `vec4 position + vec2 UV` (stride 24, offsets 0 et 16) ;
- `draw_projected_mesh`, qui réutilise l'upload/indexation bornée et refuse
  les pipelines xyz+UV ou les pipelines non texturés ;
- `create_depth_render_target`, avec image `VK_FORMAT_D32_SFLOAT`, vue depth,
  render pass à deux attachments, clear depth à 1.0 et
  `VK_COMPARE_OP_LESS_OR_EQUAL`/écriture depth activés pour les pipelines qui
  ciblent cette render target.

La fixture compile le shader SPIR-V `gl_Position = vec4(position)` généré par
le glslang local, projette une maille à partir d'un `TcamCameraState`, puis
relit le pixel central `[127,127,127,127]` sur une cible couleur et sur une
cible couleur+depth.

## Validation

```text
ac6-vulkan-backend-tests : pass
projection CPU → clip pipeline → readback : pass
clip pipeline → color+depth target → readback : pass
CTest complet avec AC6_ASSET_ROOT : 54/54 pass, 63.83 s
```

## Limite explicite

Le draw reste une passe headless qui efface sa cible à chaque appel ; cette
étape prouve le format de vertex, `w`, l'attachement depth et la durée de vie,
mais pas encore le tri/occlusion d'une frame multi-mesh, le swapchain, le
shader AC6/Xenos ou le binding MATE→NTXR. Elle ne change aucune affirmation
interactive Mission 1/Mission 2.

## Hashes

```text
include/ac6/vulkan_backend.h                 7606741048a0c37b85e4201be1498da7f419c93669bda406e44abd9f79f993ff
src/vulkan_backend.cpp                       336d606e451a2579745e8ec2bc5ebec2d586332e3899898bd5cccd2e1e62860e
tests/vulkan_backend_tests.cpp               fdbafffd567a0b39f29fe015e7340ba59bd95a496d9f32b69e5ab61e6466a90e
tests/fixtures/vulkan_clip_mesh.vert         36ecf3497aa0d09100c8c222ac2c715024038d573c2673ae6159d22fb901b8eb
tests/fixtures/vulkan_clip_mesh_spirv.h      64e890290b2243afe63a85b89d5f54d5dfb924e9ba697bd7e83a7a07144cc035
```

## Frontière suivante

Raccorder un batch de meshes dans une seule passe (depth réellement
observable), puis qualifier sur les routes DATA.TBL[9]/[10] un premier
mapping MATE→texture NTXR et son upload. Le swapchain/presentation suivra
après ce contrat headless stable.
