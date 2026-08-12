# Checkpoint 4j — jonction store-backed vers le cache Vulkan clip

Date : 2026-08-12

`RetailMission01VulkanScene` ouvre l’entrée monde 119 du cache scellé par
`RetailMission01MapRenderAssets`, sélectionne un draw accepté, résout son
primitive NDXR et son NTXR, puis copie les octets vers
`VulkanMission01ClipTexturedUpload`. La matrice objet→clip et les deux blobs
SPIR-V sont des entrées obligatoires ; aucune caméra, convention de matrice ou
shader retail n’est deviné depuis les métadonnées. La translation `.pdl` de
l’instance sélectionnée est composée explicitement avant l’upload ; elle n’est
pas perdue derrière une matrice identité.

L’objet construit un `RenderScene` à un paquet et expose des spans consommables
une seule fois par `VulkanSceneResourceCache::build_clip_textured`. Le cache
copie mesh, texture et pipeline dans des ressources persistantes ; deux
soumissions successives et `reset()` sont testés. Les identifiants de mesh,
matériau et texture sont stables et le rapport conserve le digest du cache, le
couple selector/record et `jv_eligible=false`.

Le test produit `ac6-retail-mission01-vulkan-scene` reste conditionné par
`AC6_RETAIL_CACHE` et Vulkan. Les blobs utilisés sont les fixtures de test, pas
des shaders retail livrés : cette étape ferme la jonction des ressources, pas
la qualification oracle shader/caméra ni le rendu direct de `play`.

Validation store-backed effectuée sur l’index
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85` :

```text
AC6_RETAIL_CACHE=/tmp/ac6-retail-v2-smoke \
  ac6-retail-mission01-vulkan-scene-tests
retail_mission01_vulkan_scene=pass draw=1 jv_eligible=0
```

Le readback RGBA8 headless a la taille cible 64×64 et les allocations
mesh/pipeline/texture reviennent à zéro après `reset()`.
