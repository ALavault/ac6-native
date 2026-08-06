# Cycle 712 — contrat shader retail et soumission de frame Vulkan

Date : 2026-08-03  
Périmètre : lier le contrat shader Xenos qualifié au binding matériau et à une
soumission native Vulkan complète, sans oracle interactif.

## Résultat

Le contrat `A1863AF658456A14` (vertex) / `D5B4F4A878949938` (pixel) est
maintenant une construction explicite (`ac6_d5b4_shader_contract`) avec les
faits statiques qualifiés : fetch texture `tf0`, UV interpolées, attribut UV à
l'offset 6 et niveau de mip minimal 0. `is_ac6_d5b4_shader_contract` refuse
les hashes, attributs ou flags divergents avant la soumission.

`VulkanCampaignBackend::draw_campaign_vulkan_frame` ferme ensuite le seam
suivant :

```text
CampaignVulkanFrame
  -> VulkanMaterialBinding (GIDX/format/extent/mips)
  -> NTXR décodé et uploadé
  -> descriptor Vulkan
  -> pipeline clip-space fourni explicitement en SPIR-V
  -> batch CampaignProjectedMesh indexé
  -> readback RGBA8
```

Les handles d'image et de pipeline sont temporaires et libérés après la passe.
Un shader qui ne correspond pas au contrat retail est rejeté sans allocation.
Les octets SPIR-V restent une entrée explicite et ne sont pas présentés comme
une traduction binaire du shader Xenos retail.

## Validation

```text
ac6-xenos-shader-tests       : pass
ac6-vulkan-backend-tests     : pass
CTest avec AC6_ASSET_ROOT    : 54/54 pass, 60.97 s
```

La fixture Vulkan dessine effectivement le mesh projeté avec le NTXR lié au
frame, puis vérifie le pixel central `[127,127,127,255]`. Une variante avec le
hash pixel nul est rejetée.

## Frontière explicitement conservée

Cette preuve ferme l'identité statique du premier shader et le raccord matériel
vers une frame native. Elle ne prouve pas encore :

- la parité SPIR-V/Xenos du shader retail ;
- la sélection unique d'un matériau runtime parmi les variantes MATE/NDXR ;
- la présentation swapchain/SDL ;
- l'affichage du monde et du HUD pendant Mission 1 ;
- la sauvegarde et le déverrouillage interactifs de Mission 2.

## Hashes

```text
include/ac6/vulkan_material.h  7668f144871f214fec3800cb5a5b8ac4c27bda2c80513658cb478b8274aca0d7
include/ac6/vulkan_backend.h   ce1a580691d2a722032273aebfe6a516eab81f38e0efb53292b87d5c6409497a
src/vulkan_backend.cpp         c7fd45799e3ee9d64f9c5778df9bb5d71e22c10714b38405cc37828804418aac
tests/vulkan_backend_tests.cpp a5bae5d2d35cf97ecd6b4bb43d3a9cb261ff109c4a8adbbfeea09f4de51884d8
tests/xenos_shader_tests.cpp   8561523accbb35ada48bf74b747d33b2fec0624b76a1c8a39ebfd4a87322ef70
```

## Prochaine frontière

Ajouter une frame persistante (pas de clear implicite par objet), puis une
présentation Vulkan explicite. Le raccord Mission 1 devra réutiliser ce
pipeline avec HUD/contrôles et conserver les contrats de progression sans
réintroduire de force flag.
