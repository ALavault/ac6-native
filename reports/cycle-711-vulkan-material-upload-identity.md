# Cycle 711 — garde d'identité binding matériau → upload Vulkan

Date : 2026-08-03  
Périmètre : empêcher qu'un descripteur Vulkan soit créé avec un NTXR dont le
GIDX, les dimensions ou le format contredisent le `VulkanMaterialBinding`.

## Résultat

`VulkanCampaignBackend::upload_material_texture` valide avant l'upload :

- `binding.gidx == texture.gidx` ;
- largeur et hauteur exactes ;
- correspondance BC1/BC3 annoncée par le binding ;
- au moins un niveau résident.

Une divergence est rejetée sans allocation. La fixture accepte un binding BC3
4×4 correspondant au NTXR synthétique, puis rejette le même payload avec un
GIDX différent. L'upload accepté réutilise la chaîne existante image/staging,
descriptor et lifetime.

## Validation

```text
ac6-vulkan-backend-tests : pass
CTest complet avec AC6_ASSET_ROOT : 54/54 pass, 64.54 s
```

Le gate retail du cycle 710 a déjà appelé le résolveur natif sur 1 021
bindings décodés de l'entrée 9 et 85 de l'entrée 10. Ce cycle ferme seulement
la garde au moment de l'upload GPU ; il ne donne pas encore le hash du shader
retail ni une sélection unique par objet.

## Hashes

```text
include/ac6/vulkan_backend.h     9c2601157ce06b023b1c71117b0f7b08e35b62b6a4979d4c6178f5c0045df59f
src/vulkan_backend.cpp           d1a4289babe96041e5d253a724ab7b5f55adf7acd4a96e97684cb4c66c849398
tests/vulkan_backend_tests.cpp   3dc6555186a4a5a3d3f8ca919eee8888274a56334ab4c95204186a89b1e3cc57
```

## Frontière suivante

Relier un shader contractuel qualifié et un lot de matériaux distincts à la
présentation Vulkan, puis faire un test de frame complet sans effacement entre
objets. Les gates de sauvegarde et de mission interactive restent inchangées.
