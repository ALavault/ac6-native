# Cycle 719 — soumission native monde + HUD

Date : 2026-08-03  
Périmètre : raccorder `CampaignVulkanFrame.hud` au même appel de rendu que la
batch de meshes texturés, sur une cible persistante.

## Résultat

`draw_campaign_vulkan_frame_with_hud` refuse une frame sans HUD ou des shaders
HUD vides, appelle le chemin générique qualifié
`draw_campaign_vulkan_frame`, puis crée un pipeline triangle non texturé sur
le même render pass et soumet `draw_campaign_hud_overlay`. Le matériau et le
shader retail restent vérifiés par le catalogue fourni par l'appelant; aucun
sélecteur Mission 1/2 ni force flag n'est ajouté.

La fixture Vulkan conserve les meshes précédents, exécute la frame texturée
avec sa `CampaignHudFrame`, puis lit les pixels verts de l'overlay. Le chemin
direct de l'overlay reste testé séparément afin de conserver son rejet des
cibles non persistantes et des pipelines incompatibles.

## Validation

```text
ac6-vulkan-backend-tests : pass
ac6-vulkan-sdl-window-tests : pass (skip contrôlé sous dummy)
```

La suite complète validée après ce raccord avec le même corpus PAL est
`56/56`, en 62.85 s; la fenêtre SDL reste un skip contrôlé sous
`SDL_VIDEODRIVER=dummy`.

## Hashes

```text
include/ac6/vulkan_backend.h        1b6760519d58bcec83c027f59536fe910edb08a64af149446d7c3450ee0c9d8a
src/vulkan_backend.cpp               dfaf035251e74977edd47493f2056a2f479ec80facba78a23d5d502a294b89fd
tests/vulkan_backend_tests.cpp       e7e604dab65a18695ffe08a47dc5a22fb84d2eb7d35106cc3a02bfbdad082b46
CMakeLists.txt                       cde06ac4664028113cf052f149bc43060bf005b846e973939d8c07b9b4c29b46
```

## Frontière restante

Le raccord ne rend pas encore un monde AC6 complet : il consomme la batch
projected déjà qualifiée et le matériau D5B4/NTXR fourni. La fenêtre réelle
reste dépendante du driver; il faut ensuite alimenter cette frame depuis la
session Mission 1, qualifier la caméra/LOD/ressources réellement visibles et
fermer sauvegarde, progression puis Mission 2.
