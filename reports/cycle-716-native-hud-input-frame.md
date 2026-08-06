# Cycle 716 — contrat HUD/contrôles dans la frame native

Date : 2026-08-03  
Périmètre : raccorder l'entrée XInput déjà qualifiée, le loadout, les
objectifs et la route de ressource à un état HUD renderer-neutral transporté
par `CampaignVulkanFrame`.

## Résultat

`CampaignHudInputState` applique successivement :

```text
raw XInput
  -> function_821ce088_map_buttons
  -> function_82215140_map_actions
  -> edges just-pressed/just-released
```

`build_campaign_hud_frame` expose ensuite mission/status, DPL et index
`DATA.TBL`, aircraft/weapon, compteurs d'objectifs et masques d'actions.
Une surcharge de `build_campaign_vulkan_frame` attache ce HUD à la frame native
après vérification de la ressource active et du matériau.

Le contrat est générique : aucune mission, touche SDL ou force flag n'est
introduit. Le backend Vulkan peut désormais recevoir l'état HUD avec le même
frame identity contract; le dessin de glyphes/reticules reste à raccorder.

## Validation

```text
ac6-campaign-hud-tests          : pass
ac6-campaign-vulkan-frontend-tests : pass
CTest avec AC6_ASSET_ROOT      : 55/55 pass, 61.15 s
```

La fixture vérifie A (gun) en `just_pressed`, sa libération, B (missile), le
loadout 3/4 et la progression d'un objectif. Aucun affichage retail n'est
revendiqué par ce contrat de données seul.

## Hashes

```text
include/ac6/campaign_hud.h                f1cb5d2ef86d51f789b40512021e6747e20353f037f8de2c1f4b13c0973f473e
src/campaign_hud.cpp                       0f75b69c813704f17a3a350e83e76c490ce3977d83f39264197eef5d6a95b28f
include/ac6/campaign_vulkan_frontend.h    7398bc8828c839f85971fe4e72987e4530e5b072e357f0c21e39e2a15351c6d7
src/campaign_vulkan_frontend.cpp          da1756c59d78435b0f7191bd5e4c74043f5a5203fd5c5f8de449696e49cba21e
tests/campaign_hud_tests.cpp                5c9371f0bab74cbad4a00c297d95e0f532069b6f0856b1d8cfbccef4844a3ce5
tests/campaign_vulkan_frontend_tests.cpp   26a9f91a47ff563cb4c2d161b7c016be3842ddd6fe0dc8ffc4fa017d603e2ad6
CMakeLists.txt                              e0368ee3e5ace2457ecd87642149c6aeeb7ef9f8665965cfa144a2f98ef3c996
```
