# Cycle 892 — gate différentielle native Mission 01

Date : 2026-08-04

## Résultat

`ac6-native` possède maintenant une lane fail-closed dédiée au slice Mission 01
de 1 800 ticks :

```text
ac6-native --compare-mission01 MISSION_MANIFEST 1 REFERENCE_DIR OUTPUT_DIR
```

Elle charge un replay de 30 secondes, exécute trois simulations indépendantes,
contrôle leur identité exacte, compare cinq checkpoints joueur/caméra, rend la
frame finale puis mesure SSIM, couverture et profondeur. Elle écrit la capture
native, le readback profondeur, le diff couleur et un rapport JSON. Toute
référence absente, incomplète, altérée, de mauvaise mission ou de mauvaises
dimensions est rejetée avant la simulation.

Les archives PAC/DATA ne participent pas à cette lane. Le pack de référence
local est borné à quatre artefacts identifiés par taille et FNV-64 dans un
manifeste versionné. Il n'est pas installé par CPack.

## Validation

- build normal : succès ;
- CTest sous Xvfb avec `SDL_AUDIODRIVER=dummy` : 3/3 ;
- test positif identité couleur/profondeur/checkpoints : succès ;
- tests négatifs seuil simulation et référence incomplète : succès ;
- ASan/UBSan avec détection de leaks externes désactivée : 3/3 ;
- préflight Xenia Wine officiel `16e1eb8` : prêt, service laissé inactif.

## Frontière restante

La conformité retail n'est pas acquise. Les captures jusque-là nommées
`mission-flight-candidate` montrent encore le menu de mission ; la capture
gameplay qualifiée du runtime recompilé conserve un monde noir. Il manque donc
un readback oracle positif et aligné 1280x720, ainsi que la profondeur associée.
Le LOD/index buffer exact du F-16 joueur et le premier étage noir du render
graph restent également ouverts. Le comparateur ne fabrique aucun substitut
synthétique pour franchir cette frontière.
