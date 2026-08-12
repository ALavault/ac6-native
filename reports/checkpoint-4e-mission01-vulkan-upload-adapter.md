# Checkpoint 4e — adaptateur NDXR/NTXR vers uploads Vulkan bornés

Date : 2026-08-12

Le cache Vulkan fournit maintenant une fonction d'adaptation explicite pour
les sorties déjà qualifiées des lecteurs Mission 01 :

`NdxrPosition + NdxrTexcoord + indices + DecodedTexture` devient un lot owned
`VulkanTexturedVertex + uint16 indices + RGBA8`. Les pixels NTXR sont convertis
depuis le format décodé `0xAABBGGRR` vers des octets RGBA, et toutes les entrées
sont validées avant création d'une ressource GPU.

Le périmètre est volontairement fermé : positions finies, UV finis, triangle
list sans `0xFFFF`, et `z == 0` car le backend actuel n'a pas encore le
contrat de projection/transformée 3D. Une géométrie monde ou un strip restart
retourne `nullopt` avec le test dédié; aucune coordonnée n'est silencieusement
jetée et aucun draw retail n'est déclaré rendu.

Validation ciblée :

```text
vulkan_scene_resource_cache=pass persistent_resources=1 resource_allocations_per_frame=0 transactional_refusal=1
```

Ce point ferme la conversion mémoire qualifiée jusqu'à la frontière de
projection. La prochaine divergence utile est donc la première transformée et
le premier shader Xenos réellement observés dans la fenêtre Mission 01, pas un
nouveau portage statique de la campagne.
