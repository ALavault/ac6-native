# Checkpoint 4m — staging Vulkan persistant

Date : 2026-08-12

Le présentateur Vulkan SDL ne crée plus de buffer de staging à chaque
`present_frame`. À la création de la swapchain, il alloue et mappe un buffer
RGBA8 de la taille de l'image, puis conserve aussi les buffers CPU de
conversion. Chaque présentation attend la fence précédente avant de réécrire
le buffer partagé ; `destroy()` attend le device, démappe et libère exactement
ces ressources.

Cette étape ferme l'invariant de transport « aucune allocation staging par
frame ». Elle ne transforme pas le raster CPU en renderer JV : le chemin
`play` reste explicitement la compatibilité CPU présentée par Vulkan, et les
shaders/caméra retail ne sont toujours pas qualifiés.

Le smoke de surface vérifie maintenant `persistent_upload_ready()` avant deux
présentations successives. Validation exécutée :

```text
SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir \
  reconstruction/ace-combat-6/build -R '^ac6-vulkan-surface-smoke$' \
  --output-on-failure -V
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
100% tests passed, 1 tests passed
```

La baseline complète reste verte : build `ac6-native`, CTest 81/81 (44, 79 et
81 skips qualifiés selon les ressources disponibles) et `python3 -m pytest -q`
(`141 passed, 22 subtests passed`). Les audits oracle, backend Vulkan,
contrats et frontière produit restent inchangés et passants.
