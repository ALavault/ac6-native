# Cycle 726 — frame campagne PAL présentée par SDL/Vulkan

Date : 2026-08-04  
Périmètre : faire passer une frame Mission 1 réelle (ressource PAC, matériau
NTXR, HUD et phase active après STANDBY/A) du frontend Vulkan jusqu'à une
surface SDL/X11 et une présentation swapchain.

## Plan exécuté

1. Ajouter un smoke ciblé qui réutilise le manifest/runtime/loader générique
   et la ressource `DATA.TBL[9]`.
2. Créer une fenêtre SDL Vulkan, transférer sa surface opaque au backend AC6,
   créer la swapchain et une cible persistante de 640×360.
3. Soumettre le batch texturé et le HUD avec les shaders caller-owned déjà
   qualifiés, lire la cible puis appeler `present_render_target`.
4. Conserver un skip CTest contrôlé sous `SDL_VIDEODRIVER=dummy`, et vérifier
   séparément le chemin X11/Xvfb non-dummy.

## Preuve non-dummy

```text
command context:
  DISPLAY=:99 SDL_VIDEODRIVER=x11
  Xvfb :99 -screen 0 640x480x24

output:
  vulkan_campaign_sdl_presented=1 mission=1 hud=1
  scene_changed=4439
  hud_green=4439
  world_changed=11
  textured_changed=1
  scene_draw_groups=3
  clip_x=-0.612867:-0.336477
  clip_y=0.228415:0.472099
```

La frame a réellement traversé `SDL_Vulkan_CreateSurface`,
`create_with_surface`, `create_presentation_target`, la copie vers l'image de
swapchain et `vkQueuePresentKHR`. `scene_changed` et `hud_green` proviennent
du readback de la cible avant présentation, donc le résultat n'est pas
seulement un succès de création de fenêtre.

Le smoke rejoue maintenant le groupe Scene 0, sa caméra `Tcam__c01.mop`, les
transforms `Rigid/AnimRigid` du frame 0 et les trois groupes de rendu qui en
résultent. `world_changed=11` est volontairement conservateur : il prouve que
le monde TCAM atteint la cible présentée, mais signale aussi la frontière
ouverte de cadrage/échelle, de profondeur et de matériaux pour obtenir une
silhouette lisible à résolution normale. La boucle SDL d'axes de vol et la
parité visuelle des avions blancs restent séparées.

## Validation

```text
targeted CTest : 8/8, 1 skip contrôlé sous dummy, 1.87 s
full CTest PAL : 63/63, 1 skip contrôlé sous dummy, 63.93 s
```

Le test SDL générique existant présente également sous Xvfb; le nouveau test
ajoute la ressource campagne et le HUD au même chemin. Aucun asset retail n'est
ajouté au dépôt.

## Ce qui reste ouvert

- Le chemin SDL ne consomme pas encore les axes analogiques dans une boucle de
  vol; il qualifie seulement la surface, la swapchain et une frame active.
- Le batch Scene TCAM/CUT atteint la cible, mais sa visibilité reste limitée à
  11 pixels hors HUD dans ce frame 0 (dont 1 pixel après la batch texturée)
  malgré une enveloppe projetée
  `x=-0.612867..-0.336477`, `y=0.228415..0.472099`; la topologie/LOD, la
  profondeur, l'alpha et les matériaux doivent encore être qualifiés pour une
  image de vol lisible.
- Mission 2 est déverrouillée et sélectionnable, mais son rendu présenté, son
  vol et sa complétion restent à exécuter.
- La profondeur, les contrats matériaux supplémentaires et la résolution des
  avions blancs des cutscenes restent séparés de ce smoke.

## Hashes

```text
tests/campaign_vulkan_sdl_present_tests.cpp         7fbaa018f44775bd01199dbc51ce4dbc27866ae198347457b2c0c28d6dcb16e3
CMakeLists.txt                                      beef784908c9b4452f30da58e933addf7b8d47e69ba8d929f54604211dab3851
```
