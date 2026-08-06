# Cycle 700 — upload Vulkan natif BC1/BC3

Date : 2026-08-03  
Périmètre : formats blocs Vulkan, fixture synthétique, sans oracle.

## Résultat

`NtxrDecodedTexture` accepte maintenant un champ optionnel
`compressed_blocks`. Lorsqu'il est fourni, `VulkanCampaignBackend::upload_texture`
valide la taille exacte des blocs, choisit `VK_FORMAT_BC1_RGBA_UNORM_BLOCK` ou
`VK_FORMAT_BC3_UNORM_BLOCK`, copie les blocs dans l'image optimale et conserve
le format jusqu'à la vue/descripteur. L'ancien chemin RGBA8 reste inchangé
pour les décodages portables.

Le smoke couvre les deux formats :

```text
BC1 : bloc rouge 4×4 → pixel central [255,0,0,255]
BC3 : bloc vert opaque 4×4 → pixel central [0,255,0,255]
```

Les deux images passent par le même pipeline position/UV + sampler et le même
readback. C'est la première preuve Vulkan que le backend peut conserver un
format compressé au lieu de forcer systématiquement RGBA8.

## Limites explicites

Le champ `compressed_blocks` est un contrat d'upload : le décodeur NTXR actuel
continue de produire RGBA8 et ne remplit pas encore ces blocs depuis les
payloads Xenos tuilés/endianisés. Les mips, le swizzle Xenos, les vues signed
SNORM et la résolution MATE/NDXR → shader restent à qualifier. Aucun résultat
ne revendique encore le monde gameplay ou les avions de la cinématique.

## Validation

```text
CTest : 51/51 passed
Total Test time : 40.11 s
ac6-vulkan-backend-tests : pass
git diff --check : pass
```

Hashes :

```text
include/ac6/ntxr.h                    34b6c014f383edbbbfe60ae4c608dcfe9f1c5c7e327f74fccd2c4b490257f2f1
src/ntxr.cpp                          15717a031eb8356f4277a492a08f79c2b1fb47f5d341bf1678a5082cae5087ba
include/ac6/vulkan_backend.h          2448c92f4c80e1ab5e49c8b4a70113d3252249f5c59d8413497aeb079071d1c1
src/vulkan_backend.cpp                22d8f440c4e1f09d5cd188b9941d775ccb2c54cf8b8577b4f802b6f4f20829b0
tests/vulkan_backend_tests.cpp        a64d7c9ff3c9cd1099592f0efbcce6c1326d9a255f5340daaa9d6d3484224a8e
vulkan_textured_triangle_spirv.h      3cb4e4bfd6df7156f00cae5e0e949a231b333f2ab63ba768cbd65ce6e07485fc
```
