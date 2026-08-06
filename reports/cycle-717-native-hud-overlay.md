# Cycle 717 — overlay HUD natif sur frame Vulkan persistante

Date : 2026-08-03  
Périmètre : transformer le `CampaignHudFrame` qualifié en primitives clip-space
sur une cible Vulkan persistante, sans effacement implicite de la scène.

## Résultat

`VulkanCampaignBackend::draw_campaign_hud_overlay` accepte uniquement une
cible persistante déjà initialisée et rendue, ainsi qu'un pipeline non texturé
attaché au même render pass. Il dessine une barre d'objectifs, son remplissage
borné par `completed_objectives / required_objectives`, et les indicateurs
canon Gun/Missile/Target présents dans `action_bits`. Chaque rectangle est
soumis comme deux triangles et les échecs sont propagés sans force flag.

La fixture Vulkan dessine d'abord deux meshes sur des moitiés différentes,
puis l'overlay sur la même cible; le readback conserve les deux régions et
observe des pixels verts issus du pipeline HUD. Cela ferme le raccord
HUD-data → géométrie native déterministe, mais pas la parité des glyphes,
reticules ou couleurs retail.

## Validation

```text
ac6-vulkan-backend-tests : pass
CTest avec `AC6_ASSET_ROOT` PAL : 55/55 pass, 64.12 s
headless presentation : surface créée, swapchain toujours refusé par le driver
```

Le seul skip de présentation reste celui du cycle 715
(`VK_ERROR_INITIALIZATION_FAILED`) et n'est pas converti en succès implicite.

## Hashes

```text
include/ac6/vulkan_backend.h       363d141392c39b00ecd72c7e45b34efa2942640a89f8f2c0e1ec62329ba57a2d
src/vulkan_backend.cpp              be736e5664a288c3454d1205805270f03ac8ccb7117a15edcd0f262940c967e9
tests/vulkan_backend_tests.cpp      9ff14b9f08afaebf936267407adfc59e430d09e6202b35c68cdca45f46064292
CMakeLists.txt                      e0368ee3e5ace2457ecd87642149c6aeeb7ef9f8665965cfa144a2f98ef3c996
```

## Frontière restante

Le backend ne présente encore aucune image sur une fenêtre/swapchain dans
ce driver, et le HUD ne prétend pas reproduire le layout retail. Les gates
de sauvegarde, déverrouillage Mission 2, monde gameplay et exécution
interactive Mission 1 restent à fermer avec le même pipeline générique.
