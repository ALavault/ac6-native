# Cycle 1593 — capacité du chemin runtime M01

## Résultat

Le handoff Vulkan PAL accepte le cache scellé et le snapshot runtime sans
index de draw, matrice clip ou SPIR-V fournis par l’appelant. Avec le cache
PAL qualifié, `open_runtime` construit :

* 4 226 instances de map ;
* 4 738 meshes persistants ;
* 167 textures dédupliquées ;
* 65 536 cellules terrain, par lots de 256.

Le pool de descripteurs partagé a été relevé de 64 à 256, borne explicite
au-dessus du compte qualifié. Le chargement complet de ces ressources passe
désormais sous Vulkan/llvmpipe ; aucune rasterisation CPU ni readback n’est
utilisée par ce chemin de construction.

## Fermeture honnête

`play --frames 1` initialise toujours le frontend et la session, puis termine
avec `ac6_retail=fail error=mission01_unqualified
detail=checkpoint2_scene_tcam` avant toute progression ou `PRESENT`. Le rapport
reste `complete_render_scene=false` et `jv_eligible=false` : TCAM/NFIC,
matériaux et shaders retail, unités/effets, eau/ciel et HUD GPU ne sont pas
encore joints.

## Contrôles

* build CMake : passé ;
* `ctest` PAL sous Xvfb et `SDL_AUDIODRIVER=dummy` : 87/87, zéro skip ;
* complexité C++ : passée ;
* frontière produit source/ELF : passée ;
* aucune lane du checkpoint 2 ni gate JV/JP/JG fermée.
