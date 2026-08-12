# Checkpoint 4g — cache de scène clip-space NDXR/NTXR

Date : 2026-08-12

Le transport direct Vulkan accepte maintenant une variante projetée distincte
du mesh 2D : `VulkanClipTexturedVertex` porte `x/y/z/w + UV`, et le cache garde
les meshes, textures et pipelines persistants jusqu'au `reset()`. La scène
refuse toujours les transforms non identité, les strips, les passes multiples,
le HUD et la profondeur ; aucune matrice n'est déduite de `RenderScene`.

L'adaptateur `make_vulkan_mission01_clip_textured_upload` convertit un mesh NDXR
et une texture NTXR déjà décodée avec une matrice objet→clip fournie par
`0xAABBGGRR` en RGBA8. Les strips NDXR sont convertis en triangle-list avec la
même alternance et les mêmes resets `0xFFFF` que le rasteriseur qualifié ; la
voie 2D, elle, reste stricte et refuse tout restart. Il ne qualifie donc ni la
caméra, ni les constantes Xenos, ni un shader retail : la preuve de projection
doit précéder son appel.

Validation ciblée :

```text
build reconstruction/ace-combat-6/build -j2                         pass
ac6-vulkan-backend                                                pass
ac6-vulkan-scene-renderer                                         pass
ac6-vulkan-scene-resource-cache                                  pass
ac6-cpp-complexity, scene contract, boundary source/binary        pass
```

Le readback headless retrouve le pixel texturé attendu après deux chemins
persistants ; la triangulation strip et les refus de rollback restent testés. Aucun shader
Xenos/SPIR-V retail n'est livré : les blobs historiques sous `reports/` restent
des indices `bridge`, et le couple `472913F460D4B446/8F1C48BA92C8E43E` n'est pas
promu en contrat produit sans capture oracle courante.
