# Cycle 720 — asset retail réel → frame Vulkan

Date : 2026-08-03  
Périmètre : supprimer le dernier matériau/mesh synthétique du test de frame
en joignant directement les ressources DATA.TBL décodées au backend Vulkan.

## Résultat

Le nouveau `campaign_retail_frame` parcourt FHM/MDLP, associe les modèles
NDXR au MATE adjacent, résout l'identité first-texture et cherche le GIDX dans
les NTXR du même groupe. Le résultat contient le `CampaignMesh`, le NTXR
réel, le `VulkanMaterialBinding` qualifié et l'identité MATE/NDXR.

La conversion de topologie reprend la règle déjà exercée par le shell SDL :
`primitive_flags >> 8 == 0x40` signifie triangle-list; les autres polygones
locaux sont des strips avec restart `0xffff`, triangulés avec alternance de
winding. Cela permet enfin à entry 10 de passer le même contrat que entry 9,
sans force flag ni branche Mission 2.

`upload_texture` accepte désormais le cas réel où le décodeur fournit à la
fois `compressed_blocks` et `rgba8`; le chemin BC qualifié est choisi, tandis
que le fallback RGBA reste valide lorsqu'il est le seul payload.

## Validation asset et Vulkan

```text
ac6-campaign-retail-frame-tests:
  selector 1 → DATA.TBL entry 9, polygon 8, 4 vertices / 6 indices, GIDX 268444181
  selector 2 → DATA.TBL entry 10, polygon 4, 4 vertices / 6 indices, GIDX 268444181

ac6-campaign-vulkan-retail-frame-tests:
  selector 1 → scene_changed=8568, hud_green=342
  selector 2 → scene_changed=8568, hud_green=342

CTest PAL avec AC6_ASSET_ROOT : 58/58 pass, 64.42 s
```

La caméra du test est un cadrage diagnostic borné par l'AABB du mesh; elle ne
revendique pas encore l'ordre TCAM retail. Le readback prouve le monde
texturé et le HUD sur la cible Vulkan, mais pas une fenêtre swapchain dans le
driver courant, ni le gameplay interactif.

## Hashes

```text
include/ac6/campaign_retail_frame.h       1d57b19af1c7428aadb601fe7c8aee15ea59c94985d4bea5f92708b7891afc59
src/campaign_retail_frame.cpp              58c8102c70c9539a20ab6c49c91404345e915c6bc7336c940c8003960b0c9a59
tests/campaign_retail_frame_tests.cpp     3faf184d5be854a2828f45473db666a490226ad87669b88fc78fc1c1eb333cc2
tests/campaign_vulkan_retail_frame_tests.cpp c01960a49637ef6dd0cd29061e48f23062d3d0e6610a06a7596f26474b6feb8b
src/vulkan_backend.cpp                     dca36c4b1e826bad462f43aef8ffc9d66cf0958a8479b534184d5d6f2ead54c2
tests/vulkan_backend_tests.cpp             e7e604dab65a18695ffe08a47dc5a22fb84d2eb7d35106cc3a02bfbdad082b46
CMakeLists.txt                             93afa67231745eff1f3733c7411b31975f87190abf1ef69de9e3ed23209c769d
```

## Frontière restante

Le raccord réel est encore un checkpoint renderer/offline : le cadrage AABB,
la présentation SDL/Vulkan effective, la session STANDBY→MISSION 1, les
contrôles en vol, la sauvegarde et le déverrouillage Mission 2 restent à
qualifier. La prochaine implémentation doit remplacer le cadrage diagnostic
par le TCAM de la session et conserver exactement ce chemin asset générique.
