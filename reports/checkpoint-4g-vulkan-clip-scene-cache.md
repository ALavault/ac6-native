# Checkpoint 4g — cache de scène clip-space NDXR/NTXR

Date : 2026-08-12

Le transport direct Vulkan accepte maintenant une variante projetée distincte
du mesh 2D : `VulkanClipTexturedVertex` porte `x/y/z/w + UV`, et le cache garde
les meshes, textures et pipelines persistants jusqu'au `reset()`. La scène
refuse toujours les transforms non identité, les strips, les passes multiples,
le HUD et la profondeur ; aucune matrice n'est déduite de `RenderScene`.

L'adaptateur `make_vulkan_mission01_clip_textured_upload` convertit un mesh NDXR
et une texture NTXR déjà décodée avec une matrice objet→clip fournie par
l'appelant. Il vérifie finitude, `w != 0`, indices triangle-list et convertit
`0xAABBGGRR` en RGBA8. Il ne qualifie donc ni la caméra, ni les constantes
Xenos, ni un shader retail : la preuve de projection doit précéder son appel.

Validation ciblée :

```text
build reconstruction/ace-combat-6/build -j2                         pass
ac6-vulkan-backend                                                pass
ac6-vulkan-scene-renderer                                         pass
ac6-vulkan-scene-resource-cache                                  pass
ac6-cpp-complexity, scene contract, boundary source/binary        pass
```

Le readback headless retrouve le pixel texturé attendu après deux chemins
persistants ; les refus de strip et de rollback restent testés. Aucun shader
Xenos/SPIR-V retail n'est livré : les blobs historiques sous `reports/` restent
des indices `bridge`, et le couple `472913F460D4B446/8F1C48BA92C8E43E` n'est pas
promu en contrat produit sans capture oracle courante.
