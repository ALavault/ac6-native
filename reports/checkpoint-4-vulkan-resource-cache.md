# Checkpoint 4b — cache GPU persistante de scène

Date : 2026-08-12

`VulkanSceneResourceCache` ajoute la transaction d'upload qui manquait à la
preuve de transport : les meshes et pipelines d'une scène bornée sont créés
une seule fois depuis des spans d'entrée, puis conservés par identifiant stable.
Le rendu suivant ne reçoit que le `RenderScene` scellé et réutilise les handles;
les buffers hôte d'entrée peuvent donc disparaître après `build`.

Le cache refuse explicitement les textures, les transforms non identité, les
passes multiples, le HUD et les topologies hors triangle-list. Une construction
incomplète détruit immédiatement les ressources déjà créées et laisse zéro
mesh/pipeline vivant. Un digest différent de celui qualifié à `build` est
également refusé.

Validation :

```text
vulkan_scene_resource_cache=pass persistent_resources=1 resource_allocations_per_frame=0 transactional_refusal=1
ctest -R 'ac6-vulkan-scene-(renderer|resource-cache)|ac6-vulkan-backend' : 100% (4/4)
```

Ce checkpoint ne revendique pas encore les ressources retail NDXR/NTXR ni la
parité Mission 01 : il ferme uniquement la durée de vie GPU persistante et le
refus transactionnel, avant l'ajout des layouts 3D, textures et shaders
qualifiés.
