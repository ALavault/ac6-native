# Checkpoint 4d — cache de scène texturé borné

Date : 2026-08-12

`VulkanSceneRenderer` et `VulkanSceneResourceCache` exposent maintenant une
voie texturée distincte de la voie position-only :

- `VulkanTexturedMeshHandle` conserve position et UV dans un buffer persistant ;
- `VulkanTextureHandle` référence une image RGBA8 device-local et son descriptor
  set persistant ;
- `render_textured` exige exactement une texture par `DrawPacket`, triangle-list,
  transformée identité, une passe headless et aucun HUD/profondeur ;
- `build_textured` est transactionnel : toute absence de mesh, texture, shader,
  capacité ou correspondance de contrats libère les ressources déjà créées.

Le test headless construit le cache depuis des spans temporaires, dessine deux
fois après l'upload, valide le pixel central rouge par readback puis vérifie que
`reset()` libère mesh UV, texture et pipeline. Aucun staging n'est conservé et
aucune allocation n'est effectuée pendant le dessin.

Validation ciblée :

```text
vulkan_scene_resource_cache=pass persistent_resources=1 resource_allocations_per_frame=0 transactional_refusal=1
```

La voie ne revendique pas encore la traduction des shaders Xenos, les
constantes, les transforms 3D, la profondeur, les passes intermédiaires ou le
swapchain. Les ressources retail NDXR/NTXR restent donc une source qualifiée à
adapter, pas une dépendance implicite du cache.
