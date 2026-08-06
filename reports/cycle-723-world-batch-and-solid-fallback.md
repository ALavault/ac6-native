# Cycle 723 — batch monde Mission 1 et fallback géométrique Vulkan

Date : 2026-08-03  
Périmètre : remplacer le mesh unique sous-pixellaire par le batch Scene réel,
et rendre les mailles sans jointure diffuse MATE/NTXR sans fabriquer de
texture.

## Plan exécuté

1. Corriger la conversion des triangle strips AC6 : les restart `0xffff`
   sont retirés et le `index_count` borné est recalculé avant la validation du
   mesh.
2. Exposer la résolution de tous les joins
   `Scene asset key → NDXR polygon → MATE → NTXR`, en conservant l’ordre des
   batches et les GIDX réels.
3. Exposer séparément la géométrie d’un asset sans diffuse qualifié. Cette
   voie ne revendique ni texture ni matériau retail.
4. Ajouter un pipeline clip-space solide sans descriptor Vulkan afin de
   soumettre le décor restant sur la même cible persistante.
5. Rejouer `DATA.TBL[9]` depuis le manifest/runtime, STANDBY puis edge A,
   TCAM/CUT frame 0, et vérifier le readback du monde et du HUD.

## Résultats PAL

```text
selector 1 / DATA.TBL entry 9:
  STANDBY HUD readback                         300 pixels verts
  camera                                      Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop
  world transforms                             16
  textured Scene polygon parts                 115
  textured Vulkan draw groups                  2
  untextured geometry parts                   91
  solid fallback draw group                    1
  active scene_changed                         3 pixels à 128x128
  active HUD readback                          303 pixels verts

selector 2 / DATA.TBL entry 10:
  même loader/manifest/runtime/frontend        true
  active scene_changed                         166
  active HUD readback                          300 pixels verts
```

Les 115 polygones texturés correspondent aux deux avions (`r_f16c` et
`r_f18f`). Les 91 mailles solides sont une preuve de soumission de géométrie
sans diffuse : elles ne constituent pas encore une preuve de matériau, de
shader Xenos, d'occlusion/depth ou de parité visuelle retail.

## Validation

```text
targeted CTest : 4/4 pass, 2.21 s
full CTest PAL : 59/59 pass, 63.05 s
```

La suite backend couvre désormais aussi un batch clip-space sans texture.
Le contrat de compatibilité existant `create_mesh_pipeline`/`draw_indexed_mesh`
reste texturé et passe sans régression.

## Ce qui reste ouvert

Ce checkpoint ferme la soumission d’un monde Scene multi-polygones sur une
cible Vulkan persistante, pas une fenêtre présentée ni un vol interactif. Les
contrôles pitch/roll/yaw/throttle, objectifs, snapshot de sauvegarde, reprise
retail et déverrouillage de Mission 2 restent à qualifier. Les assets dont la
clé ou le payload ne sont pas encore joignables restent explicitement absents
du batch ; aucune donnée retail n’est ajoutée au dépôt.

## Hashes

```text
include/ac6/campaign_retail_frame.h          800466650d69b09eb12e05fc1524be43ed6a1d6c1105beecf6c40ba338b9bb92
src/campaign_retail_frame.cpp                 3123bec5a747d24c6b5d7f980d252222912cc6f736d9f0334487652ff0634622
include/ac6/campaign_scene_frame.h            2d5203dfc35b73cb43b87a65e5d180560c8ade492190454cd7be5e7cbdf74baf
src/campaign_scene_frame.cpp                  78465fb694fb52ebdf91be18490ace820d9033eb8d52369a8f0876207375f6b8
include/ac6/vulkan_backend.h                  55be1188e6e489a07500b02aca77a284bbeb1e8e49cbc86f6400e8605e996895
src/vulkan_backend.cpp                        e797bf81f1f95db90b6114ed2dabe855086ec8479ebb7156713a6fbb649e73ba
tests/campaign_scene_frame_tests.cpp          0e8508f29fee3cbe062136a0d7cd308e48bbb2c47d1ce186002c985aeb20512b
tests/campaign_vulkan_retail_frame_tests.cpp  1b9f0ee99dde12bc976ab4d37300b05fe120c5f15cd970ca017f4b1a7d872e87
tests/vulkan_backend_tests.cpp                2d3ec775000d2fd71d7711fd669f0ce39adab52a87a6f0270c312a8d27b89877
```
