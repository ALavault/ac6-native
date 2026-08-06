# Cycle 706 — vertex input mesh Vulkan `xyz + uv`

Date : 2026-08-03  
Périmètre : shader SPIR-V synthétique, vertex input explicite et draw mesh
indexé.

## Résultat

Une fixture glslang qualifiée (`vulkan_mesh.vert`) ajoute une entrée
`location=0 vec3 position` et `location=1 vec2 texcoord`, avec
`gl_Position=vec4(position,1)`. Le backend expose `create_mesh_pipeline`,
distinct de la pipeline triangle 2D :

```text
stride = 5 × float = 20 octets
location 0 : R32G32B32_SFLOAT, offset 0
location 1 : R32G32_SFLOAT,    offset 12
```

La pipeline réutilise le fragment shader texturé déjà vérifié et le
`draw_indexed_mesh` du cycle 705. Un quad 4 vertices/6 indices est uploadé,
rasterisé et relu avec la couleur attendue. Le test conserve les rejets des
indices hors bornes et des listes qui ne sont pas des multiples de trois.

## Validation

```text
ac6-vulkan-backend-tests : pass
mesh SPIR-V + pipeline stride 20 + quad indexé : pass
CTest complet avec AC6_ASSET_ROOT : 53/53 pass, 40.14 s
whitespace check : pass
```

Hashes :

```text
include/ac6/vulkan_backend.h       e796f2f4252621d0c4fa04a2f250c66c1cbdc9e37ca983544c385d5eb1c17da2
src/vulkan_backend.cpp              eb48e7ee5fe15ecbc65194be8a46321bb6d4137937b69d86a6c0fbfa7f50057f
tests/vulkan_backend_tests.cpp      c512695a0cd3a620affe2ae1234c0d5014021d034ffd2b8575fa527ae43b856f
tests/fixtures/vulkan_mesh.vert     ed1d2615d5034e0788de406bd70d3d51c6bedabc9e3728251f9a13609f0009e3
tests/fixtures/vulkan_mesh_spirv.h  330c0b72b9c2169820a45dd9fe8d83951db8cb1ebe52c45e3039e3141f089649
```

## Limite explicite

Le layout est une fixture Vulkan portable, non une attribution des offsets
Xenos au NDXR retail. Le `CampaignMesh` CPU n’est pas encore branché à cette
API sans projection choisie par le caller ; les `primitive_flags`, matrices,
profondeur, shaders AC6 et swapchain restent à qualifier. Aucune preuve
interactive Mission 1/Mission 2 n’est modifiée.
