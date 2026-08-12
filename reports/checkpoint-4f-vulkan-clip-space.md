# Checkpoint 4f — layout clip-space Vulkan pour caméra fixe

Date : 2026-08-12

Le transport AC6-owned récupère le layout générique historique `vec4 position +
vec2 UV` sous des types distincts :

- `VulkanClipTexturedVertex` et `VulkanClipTexturedMeshHandle` ;
- pipeline avec attribut position `R32G32B32A32_SFLOAT` et UV à l'offset 16 ;
- `draw_clip_textured_indexed` exige un pipeline clip-space, une texture
  persistante et des layouts d'image valides.

Le test headless soumet un triangle en coordonnées clip déjà projetées avec le
fixture SPIR-V correspondant, puis libère pipeline, texture et mesh. Le chemin
position/UV 2D refuse ce pipeline, ce qui maintient une frontière de type
explicite.

Validation ciblée :

```text
vulkan_backend=pass device=NVIDIA RTX PRO 4000 Blackwell api=1.4 depth_d32=1
```

Cette preuve ferme uniquement le transport de caméra fixe déjà projetée. Elle
ne qualifie pas encore la matrice vue/projection, les constantes, les
transforms vivantes ou un shader Xenos retail; aucune géométrie monde n'est
redirigée vers ce chemin sans cette preuve.
