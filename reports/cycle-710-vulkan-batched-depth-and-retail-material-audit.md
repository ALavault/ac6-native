# Cycle 710 — batch depth Vulkan et audit MATE/NDXR retail

Date : 2026-08-03  
Périmètre : rendre observable l'occlusion entre plusieurs meshes dans une
même passe, puis mesurer le lien structurel MATE→NDXR sur les ressources
réelles des routes 9 et 10.

## Résultat renderer

`VulkanCampaignBackend::draw_projected_mesh_batch` concatène des
`CampaignProjectedMesh` bornés, rébase les indices `uint16_t`, vérifie les
limites et soumet un seul draw indexé. Avec la cible `D32_SFLOAT`, deux
triangles superposés (lointain rouge, proche vert) produisent un pixel central
vert : l'occlusion depth est donc observable dans une même render pass.
Les meshes d'un batch partagent volontairement le descripteur texture du
pipeline ; un changement de matériau reste une frontière de batch explicite.

## Résultat corpus PAL

Le test file-backed parcourt désormais les MDLP/FHM des deux payloads réels et
essaie les paires candidates MATE/NDXR dont le nombre de batches correspond
exactement au nombre de polygones. Il conserve une preuve structurelle, pas une
affirmation de sélection runtime :

```text
selector 1 / DATA.TBL[9] : 42 meshes texturés
  379 payloads MATE, 1435 jeux de bindings candidats
  1885 correspondances first-texture MATE/NDXR
  86 conteneurs NTXR candidats, 1021 bindings natifs résolus
selector 2 / DATA.TBL[10] : 8 meshes texturés
  120 payloads MATE, 398 jeux de bindings candidats
  294 correspondances first-texture MATE/NDXR
  23 conteneurs NTXR candidats, 85 bindings natifs résolus
```

Le résolveur natif est appelé avec les identifiants first-texture correspondants
et des textures BC1/BC3 effectivement décodées depuis les conteneurs NTXR
adjacents. Le contrat de shader fourni à ce résolveur reste synthétique
(UV/fetch présents, mips 0..0, vue unsigned) : les nombres de candidats peuvent
compter plusieurs variantes dans le même conteneur et ne prouvent ni le choix
d'un seul MATE par objet, ni le shader Xenos ou son hash.

## Validation

```text
ac6-vulkan-backend-tests : pass
ac6-campaign-retail-asset-tests : pass
CTest complet avec AC6_ASSET_ROOT : 54/54 pass, 63.82 s
```

## Hashes

```text
include/ac6/vulkan_backend.h           20afbb1ef7f1484d84cdd0058242adc55b433a9105d8d409417f244dc1a14ca3
src/vulkan_backend.cpp                 158c2bc3f39c37bf8940de62370bf8cf13ec7fe37f5fa6796a8c2810155bccd7
tests/vulkan_backend_tests.cpp         df7080e7ca832e4b0482053c39ae0db42090895fbfd9381f6e6b5fc4e88495a8
tests/campaign_retail_asset_tests.cpp ae96f2e04a2bb8c718db675685d86992b88dedfcd0ebd00debccb46b1be86752
```

## Frontière suivante

Introduire un batch de matériaux où chaque binding est validé par
`resolve_vulkan_material_binding`, décoder seulement les NTXR correspondant
aux identifiants MATE/NDXR prouvés, puis attacher cette sélection au rendu
retail. La présentation swapchain et la qualification interactive Mission 1
restent séparées.
