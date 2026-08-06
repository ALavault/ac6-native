# Cycle 713 — frame Vulkan persistante

Date : 2026-08-03  
Périmètre : supprimer le clear implicite entre lots de géométrie sur une
cible native Vulkan, tout en gardant un clear initial explicite et traçable.

## Résultat

Le backend expose désormais `create_persistent_render_target` et
`create_persistent_depth_render_target`. Ces cibles créent des attachments
`LOAD`; `clear_render_target` effectue alors les transitions image et les
`vkCmdClear*` explicites avant le premier lot. Un draw sur une cible persistante
non initialisée est rejeté. Les cibles historiques restent en mode `CLEAR`.

La fixture soumet deux meshes projetés séparément sur la même cible couleur et
observe les deux régions dans le readback. Elle répète la preuve avec une cible
profondeur persistante. Cela distingue le nouveau contrat d'un simple batch :
la seconde soumission ne peut pas effacer la première.

## Validation

```text
ac6-vulkan-backend-tests  : pass
CTest avec AC6_ASSET_ROOT : 54/54 pass, 60.72 s
```

## Frontière conservée

Cette preuve ferme le lifetime de frame côté render target, mais pas encore la
présentation swapchain/SDL, la synchronisation multi-frame, le HUD natif ou le
rendu du monde de Mission 1. Le prochain changement doit brancher ce contrat à
une surface Vulkan explicite sans réintroduire de clear par objet.

## Hashes

```text
include/ac6/vulkan_backend.h   9da7550426b06a16e23e4e3d47edb2f0f4e3cb4a3fa1232611b6310ab2768954
src/vulkan_backend.cpp         75e31515c3ef9fe1042e6e499fab57cc8b7d7177011ed8c224152dd01e1202a6
tests/vulkan_backend_tests.cpp 1ce6dc7d5b50fae949e69a5b4be66550c748ce324501c825301e33df89de2c68
```
