# Cycle 714 — catalogue générique de contrats shader

Date : 2026-08-03  
Périmètre : retirer le branchement D5B4 spécifique du backend et qualifier les
bindings retail avec un catalogue de contrats extensible.

## Résultat

`is_vulkan_shader_contract_qualified` compare un shader à un catalogue fourni
par l'appelant. `draw_campaign_vulkan_frame` exige désormais ce catalogue et ne
contient plus de branche Mission 1/D5B4. Le premier catalogue contient
`ac6_d5b4_shader_contract()`; Mission 2 pourra ajouter un contrat nouvellement
qualifié sans modifier le renderer.

Le gate `campaign_retail_asset_tests` utilise maintenant ce contrat D5B4 réel
pour le parcours MATE→NDXR→NTXR, au lieu d'un shader synthétique aux hashes
nuls. Les mêmes 1 021 bindings entry 9 et 85 bindings entry 10 sont acceptés,
avec les identités GIDX/format/extent/mips déjà vérifiées.

## Validation

```text
ac6-xenos-shader-tests       : pass
ac6-vulkan-backend-tests     : pass
ac6-campaign-retail-asset-tests : pass
retail_asset_pipeline_ok ... native1=1021 ... native2=85
CTest avec AC6_ASSET_ROOT    : 54/54 pass, 61.12 s
```

La présentation Vulkan, le choix unique d'un matériau runtime et le shader
SPIR-V correspondant octet par octet restent hors de cette preuve.

## Hashes

```text
include/ac6/vulkan_material.h         e6a3904cd2520ad1d03fa5de8fe6fee8039d869327c027dc777c16c1cef27ee2
src/vulkan_material.cpp               c9ac4366d1b3fcbe39d6090bb7d1869142d4b998286b397d1961b3d2aab070ab
include/ac6/vulkan_backend.h          04f4acc4331afc7dfaf25ca712fc28b5995d1d37cb1c1b3862f5a74fe5bd6c7c
src/vulkan_backend.cpp                ed1f64b9507d107116802f4d36fa676fe2ef589acdd6cc32f7a39eefbeca62f1
tests/vulkan_backend_tests.cpp        372866e9b0e8649f189af334d3d30fef92f636e8c691a5a7470eab8fa0bb471c
tests/xenos_shader_tests.cpp          c037ab52fbd7d3853b68b563ca27d38ed1c6f55797e8551cf912497346ba74c8
tests/campaign_retail_asset_tests.cpp 28e5f5b579e52ca40467090c47a32c8b4ec40fb2d913d336b97b64c3fac7f9c6
```
