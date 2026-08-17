# Cycle 1591 — M01 runtime scene boundary

## Delivered

Le chemin Vulkan produit appelle désormais `RetailMission01VulkanScene::open_runtime`
avec le cache PAL et un `SimulationSnapshot`. Il ne fournit plus d'index de
draw, de matrice clip ou de SPIR-V à l'adaptateur. Le snapshot est aussi le
canal de mise à jour du tick et de la caméra ; `VulkanSceneResourceCache` garde
les ressources persistantes et expose `render_dynamic` pour ces seules données
dynamiques.

L'adaptateur construit les paquets monde à partir des 4 226 placements, les
surfaces NTXR dédupliquées et des lots terrain de 256 cellules. La scène reste
explicitement non qualifiée : les producteurs TCAM/NFIC, unités/effects,
matériaux/shaders retail, eau, HUD GPU et preuve de projection ne sont pas
fermés. Le rapport ne met donc pas `complete_render_scene` ni `jv_eligible` à
vrai.

Le mode public `play` initialise le frontend/session puis refuse avant toute
progression ou image si ce rapport n'est pas complet et qualifié. La capture
CPU existante reste un chemin de diagnostic nommé, hors preuve JV.

## Contrôles

* build `cmake --build reconstruction/ace-combat-6/build -j16` : passé ;
* audit de complexité C++ : passé ;
* tests ciblés scène/render/cache/session : passés ;
* audit de frontière produit source + ELF : passé ;
* checkpoint 2 : toujours `open`, 0/6 lanes ;
* aucun changement de ladder ni promotion `supported=yes`.

## Reste à fermer

Le paquet terrain doit encore être relié aux matériaux et shaders retail
qualifiés, puis les unités, effets, eau, ciel, HUD et transitions doivent
entrer dans le même `RenderScene`. La scène TCAM et les preuves JV/JP/JG
restent ouvertes.
