# Cycle 722 — STANDBY runtime → frame Vulkan

Date : 2026-08-03  
Périmètre : fermer la frontière d’état `MISSION 01 / STANDBY` sans force
flag, puis faire porter la frame Vulkan par le runtime/manifest/loader réel.

## Implémentation

`CampaignRuntimeState` possède maintenant une phase native distincte du statut
de progression :

```text
idle → loadout → briefing → standby → active → complete
```

`enter_campaign_runtime_standby()` expose l’écran STANDBY après un loadout
qualifié. `launch_campaign_runtime_standby()` exige explicitement l’edge A
(action slot 0 des bindings PAL) ; un appel sans cet edge et un appel direct à
`start_campaign_runtime_mission()` depuis STANDBY sont refusés. Le passage en
actif ne touche ni mémoire guest ni renderer.

`CampaignVulkanFrame` transporte cette phase. La fixture Vulkan retail ne
fabrique plus la frame : elle construit un manifest depuis `DATA.TBL`, charge
la ressource par `CampaignRuntimeState`, équipe le loadout, construit la frame
STANDBY avec le HUD, la soumet sur une cible distincte, puis injecte l’edge A
et soumet la frame active sur le même chemin. Le selector 2 suit le même
pipeline générique.

## Validation

```text
selector 1 / DATA.TBL 9:
  phase standby construite = true
  lancement sans A = rejeté
  lancement avec A = active
  STANDBY HUD readback = 300 pixels verts
  TCAM = Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop
  world transforms = 16
  active scene_changed = 0 à 128x128 (mesh initial sous-pixellaire)

selector 2 / DATA.TBL 10:
  même manifest/loader/runtime/frontend
  active scene_changed = 8568
  HUD readback = 300 pixels verts

Tests ciblés : 6/6 pass, 1.12 s
Full CTest PAL avec `AC6_ASSET_ROOT` : 59/59 pass, 62.89 s
```

Ce cycle ferme uniquement le contrat déterministe d’état et de soumission
STANDBY. Il ne revendique pas une fenêtre swapchain, un vol interactif, un
monde Mission 1 non noir à résolution normale, une sauvegarde retail ou le
déverrouillage de Mission 2.

## Hashes

```text
include/ac6/campaign_runtime.h              5b352a864c9d87fba48c9b0ef77a117c8b7a3dd9a6788b10e341541160db0261
src/campaign_runtime.cpp                     63e6e310aeb49d4add5199d9ff6780960357ffdce910b78c81f2a9f640fbe85a
include/ac6/campaign_vulkan_frontend.h      b7e4f9bea904899ce6d9170a82c3b02657c3ebc63ca765d87cad93547f8f84cb
src/campaign_vulkan_frontend.cpp             e230216f870dd175759244d196446364b04b279bdcb1bdf59d5eadecfc030c8a
tests/campaign_runtime_tests.cpp              addfff39f7f7e134fed675f17a9dc42f5a7111cf734c7e87ea3fa0ff85732085
tests/campaign_vulkan_frontend_tests.cpp     5445df628ceb3e162e8f50655d7f8e580ae59ad4abda9c9e1bd0ef2e9d5d7344
tests/campaign_vulkan_retail_frame_tests.cpp e60fc4c1c706afaca24c268a8a5faa37ec58a588ad052ad251a2202cdc3a060a
CMakeLists.txt                               8784567665fb1e7707b11a984a8c563a31b6fff1b9367d77fd802fbaff5be7b5
```

## Frontière suivante

Conserver le gate A et rattacher la frame active à un batch de monde assez
large pour une preuve non noire à résolution normale, puis brancher objectifs,
contrôles de vol et snapshot de sauvegarde. Le même contrat devra restaurer
la progression et débloquer Mission 2 sans branche renderer spécifique.
