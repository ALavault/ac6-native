# Cycle 1538 — les DrawPacket monde atteignent Vulkan sans préprojection CPU

Date : 2026-08-12

## Résultat

Le backend Vulkan possède désormais une voie typée distincte pour les sommets
`XYZ+UV`. Les positions NDXR restent dans l'espace auteur jusqu'au vertex
shader : aucun sommet n'est projeté ni rasterisé sur CPU. La caméra générique
de `RenderScene` est convertie une fois par soumission en matrice
`world_to_clip`, puis composée avec le `DrawPacket::transform` propre à chaque
draw. La matrice `object_to_clip` résultante est transmise par une push
constant de 64 octets.

Cette voie exige une cible D32, le test et l'écriture de profondeur, et garde
la profondeur entre les render passes de packets successifs. Meshes, textures
et pipelines sont créés dans le cache persistant ; `render()` ne reconvertit
ni ne réalloue les ressources.

Le slot texture 0 dynamique est explicite dans `RenderScene` par la sentinelle
`@vulkan.draw-packet.texture0`. Il permet à plusieurs packets partageant le
même pipeline de sélectionner des textures différentes sans prétendre que le
matériau est lié à une ressource fixe.

## Contrôles positifs

Le contrôle GPU soumet trois packets sur un mesh et un pipeline :

- un triangle rouge au centre ;
- un triangle vert translaté par sa matrice objet et plus proche ;
- un triangle rouge au même emplacement mais plus loin.

Les positions auteur ont `z=10`, hors du volume clip si la caméra était
ignorée. Le readback retrouve le rouge au centre et le vert au pixel translaté
: la caméra non identité, la translation par draw et l'ordre D32 sont donc
tous observables. Le cache compte exactement un mesh, un pipeline et deux
textures, puis zéro ressource après `reset()`.

L'adaptateur M01 conserve aussi `x/y/z`, convertit les strips à restart en
triangles bornés et copie les pixels NTXR décodés vers un upload propriétaire.

## Refus et transaction

La voie échoue avant soumission pour une matrice non finie, une caméra
coïncidente ou colinéaire, un FOV hors `(0, pi)`, une cible sans D32, le
culling, le wireframe, le reverse-Z, le blend non implémenté, des constantes
matériau, un sampler explicite, une anisotropie différente de 1 ou une liaison
texture fixe mensongère. Une erreur de shader après création des mesh/textures
libère toutes les ressources partielles.

## Validation

```text
build backend/renderer/cache/native                         pass
CTest render-scene + Vulkan                                4/4
VK_LAYER_KHRONOS_validation                                pass
GPU                                                        NVIDIA RTX PRO 4000 Blackwell
Vulkan API                                                 1.4
cache PAL M01, ancien clip path                            pass, JV eligible = 0
git diff --check                                           pass
```

## Frontières conservées

Ce lot fournit le transport monde, pas M01-B ni JV. Le frontend interactif
présente toujours son frame CPU ; les 4 226 placements de ville ne sont pas
encore assemblés dans ce cache. La caméra est la caméra générique du renderer,
pas encore le producteur TCAM retail. Les shaders du contrôle sont des
fixtures ; les shaders, constantes, matériaux, états raster et caméra retail
restent à qualifier avant de raccorder `play`.
