# Cycle 727 — axes SDL, sauvegarde AC6S et présentation Mission 2

Date : 2026-08-04  
Périmètre : fermer le raccord SDL/gamepad → axes natifs → pose TCAM, puis
rejouer la progression persistée et présenter Mission 2 avec le même pipeline
PAC/Scene/HUD/Vulkan.

## Preuve non-dummy

Commande exécutée sous Xvfb/X11 :

```text
DISPLAY=:99 SDL_VIDEODRIVER=x11
AC6_ASSET_ROOT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/game-files
ac6-campaign-vulkan-sdl-present-tests
```

Sortie déterministe :

```text
vulkan_campaign_sdl_presented=1 mission=1 hud=1
scene_changed=4439 hud_green=4439 world_changed=11 textured_changed=1
scene_draw_groups=3 clip_x=-0.612867:-0.336477
clip_y=0.228415:0.472099 flight_changed=1 flight_world_pixels=0
flight_pixels=4428 mission1_completed=1 mission2_restored=1
mission2_presented=1 mission2_changed=6974 mission2_hud_green=4428
```

La fixture pousse quatre événements `SDL_GAMEPAD_AXIS_MOTION`, les collecte
dans `CampaignFlightHostAxes`, les normalise dans le contrat SDL-indépendant
`CampaignFlightInput`, puis avance huit pas bornés de 1/30 s avant de reprojeter
le mesh Scene avec la caméra TCAM modifiée. `flight_changed=1` prouve la chaîne
événement → axes → état natif → projection; ce n'est pas une récupération des
équations de vol retail.

La Mission 1 passe `STANDBY → A → active`, termine son objectif et encode un
snapshot `AC6S` de 28 octets. Le snapshot est écrit puis relu depuis disque,
restauré dans un runtime neuf, et déverrouille Mission 2. Mission 2 est ensuite
sélectionnée avec le même manifest, les mêmes sources PAC, le même loader et le
même backend; son mesh diagnostique et son HUD atteignent la même swapchain.

`flight_world_pixels=0` est conservé comme résultat négatif utile : la pose
projetée change, mais le premier groupe texturé choisi pour ce readback ne
produit aucun pixel monde. Le chemin Scene principal reste toutefois positif
(`world_changed=11`); cette différence borne la prochaine investigation
alpha/matériaux/topologie/profondeur au lieu de masquer le défaut par un
force flag ou un LOD artificiel.

## Validation

```text
build : cmake --build .../reconstruction-material -j2
targeted CTest : 8/8, 1 skip contrôlé sous SDL_VIDEODRIVER=dummy, 1.80 s
full CTest PAL : 63/63, 1 skip contrôlé sous SDL_VIDEODRIVER=dummy, 63.13 s
```

Le test SDL non-dummy est exécuté explicitement sous Xvfb; le skip CTest reste
intentionnel pour le driver dummy sans surface Vulkan présentable. Aucun asset
retail n'est ajouté au dépôt.

## Claims et frontières

Fermés : normalisation d'axes hôte, injection SDL synthétique, changement de
projection en vol, complétion native de Mission 1, I/O fichier `AC6S`, reprise
transactionnelle, déverrouillage et présentation Mission 2 par le pipeline
commun.

Non fermés : boucle SDL/gamepad physique persistante, visibilité monde lisible
en vol (`flight_world_pixels=0`), équations de physique retail, vol et
complétion de Mission 2, profondeur et parité des matériaux/avions des
cutscenes, compatibilité d'une sauvegarde retail.

## Hashes

```text
include/ac6/campaign_flight.h                 20945a9612531a0c186209036f927001fec34495f6f8bd9d014a59634d9d1515
src/campaign_flight.cpp                       519bfa9e1f3973b8c0daef7458cfd16f7c7d9142bd7d0e74ab5920b429c9dc7d
tests/campaign_flight_tests.cpp               788a8d1f92655155e3e1a98c0280e47928bee72e46ec6a20bc15f0b6ee3dc219
include/ac6/campaign_save_io.h                3622448d6d0898ef86679322fa2de64cb338d714088146b2b54498477e628d99
src/campaign_save_io.cpp                      5cda47484e98c54b1dd68128332214da5627c2d61db303cbe0ae4a01295f66ed
tests/campaign_save_io_tests.cpp              505abd6e26d30aaabd298487ac9243cd298f8303f95911345927428f7e4f015e
tests/campaign_vulkan_sdl_present_tests.cpp   03c77ac14053ca52fb1c66dfd81479e7ee99f941a67d00d696b8dce6fc68a4f6
CMakeLists.txt                                beef784908c9b4452f30da58e933addf7b8d47e69ba8d929f54604211dab3851
```
