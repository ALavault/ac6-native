# Cycle 704 — contrat mesh NDXR sur payloads PAL réels

Date : 2026-08-03  
Périmètre : positions/UV/indices NDXR, préparation native bornée et
corrélation aux ressources campagne 1/2.

## Résultat

Le nouveau contrat `CampaignMesh` convertit un `NdxrPolygon` en flux portable
`position.xyz + uv`, conserve l’identifiant texture de tête et les
`primitive_flags` sans les renommer, et valide :

* cohérence `vertex_count/index_count` ;
* présence optionnelle des UV selon le chemin demandé ;
* indices strictement inférieurs au nombre de vertices ;
* coordonnées flottantes finies ;
* échec d’allocation signalé sans exception sortante.

Le test file-backed existant inspecte maintenant les FHM/MDLP des deux
ressources réellement décodées :

```text
selector 1 → DATA.TBL[9]  : 42 mesh contracts, 42 textured
selector 2 → DATA.TBL[10] :  8 mesh contracts,  8 textured
```

Il ne donne aucun rôle gameplay à ces meshes et ne suppose pas la topologie
des `primitive_flags`.

## Validation

```text
ac6-mesh-layout-tests : pass
ac6-campaign-retail-asset-tests + corpus PAL : pass
sortie : retail_asset_pipeline_ok ... mesh1=42 textured1=42 mesh2=8 textured2=8
CTest complet avec AC6_ASSET_ROOT : 53/53 pass, 39.88 s
whitespace check : pass
```

Hashes :

```text
include/ac6/mesh_layout.h             282191848de97ea42ee17e2f9dc97f45420e2d66afb76f5faba96a683450543d
src/mesh_layout.cpp                   2f522feaa5bee4669d15dd03035ad15766fac5e0595ed7816e667c570d27a671
tests/mesh_layout_tests.cpp           f929dd5b065f0a3e8866449787e7b31e9731fb100b91cfb548a8d297ff678b22
tests/campaign_retail_asset_tests.cpp 34e8860c366da87b31f65872a6c371c8df8c56835dc3419be0771ba8dc16af4b
CMakeLists.txt                        d640395c752578817712d7c44e710839beb65436af6d7be7ac5460c6336fc448
```

## Limite explicite

Le contrat ne décide toujours pas si `primitive_flags` décrit une liste, une
strip ou une autre primitive, et ne déduit aucun layout de registres Xenos.
Le backend Vulkan n’accepte donc pas encore ce flux comme draw AC6 : mesh
GPU, shader qualifié, profondeur, matériaux et présentation restent les
prochains seams. Les preuves interactives Mission 1/Mission 2 et la sauvegarde
retail restent inchangées.
