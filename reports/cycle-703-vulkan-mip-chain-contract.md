# Cycle 703 — chaîne de mips Vulkan générique et garde Xenos

Date : 2026-08-03  
Périmètre : upload Vulkan AC6-owned, niveaux de texture et validation
fail-closed.

## Résultat

`NtxrDecodedTexture` peut désormais porter une chaîne facultative de
`NtxrDecodedMipLevel`. Quand elle est fournie, le niveau 0 doit correspondre à
l’extension de base et chaque niveau suivant doit être `max(1, précédent/2)`.
Chaque niveau est soit RGBA8, soit le même format BC1/BC3 que la base ; les
tailles attendues sont recalculées (blocs compressés arrondis au bloc 4×4),
et toute chaîne incohérente est rejetée avant création Vulkan.

`VulkanCampaignBackend::upload_texture` concatène les niveaux dans un staging
unique, crée l’image avec le nombre réel de `mipLevels`, crée une vue couvrant
tous les niveaux et émet une transition/copie bornée par sous-ressource. Le
compteur est exposé pour les tests. Le chemin historique base-only reste
inchangé pour les payloads NTXR actuels.

La fixture Vulkan exerce une chaîne RGBA8 8×8 → 4×4 → 2×2 → 1×1, vérifie le
rejet d’un niveau 3×4 invalide, confirme quatre niveaux résidents et
échantillonne la chaîne via le descripteur. Les niveaux portent la même couleur
afin de tester le transport/layout sans prétendre qualifier le LOD du shader.

## Validation

```text
ac6-vulkan-backend-tests : pass
ac6-ntxr-tests : pass
CTest complet avec AC6_ASSET_ROOT : 52/52 pass, 40.75 s
whitespace check : pass
```

Hashes :

```text
include/ac6/ntxr.h          582820a09198cac7c19dca300dda5ac582a23a9d66673c25d640377082937df8
include/ac6/vulkan_backend.h 1178f4f33900353dd428bfb0e4e12d7bab9ddc567ebd2a8c8170856e176483ee
src/vulkan_backend.cpp      d9abcebe2a217783e567cd0752aff4ea9033973cbc62eda1b6d32e332b7962dd
tests/vulkan_backend_tests.cpp a4017365ff1b39aa6dfdf6ee84a03bba63e849ecb10c101b5635375b989e20f7
```

## Limite explicite

Le décodeur NTXR retail ne remplit toujours pas `mip_levels` : les champs
Xenos qui déterminent dimensions, ordre, format et résidence des mips ne sont
pas qualifiés. Ce cycle ferme seulement le contrat de transport Vulkan et ne
résout ni les textures blanches des cutscenes, ni le monde noir du gameplay,
ni la présentation/swapchain. Il n’ajoute aucune branche Mission 2.
