# Cycle 894 — slices NDXR retail vers renderer natif

Date : 2026-08-04

## Résultat

Le loader natif accepte désormais les NDXR binaires big-endian observés dans
les extractions bornées. L’indexeur
`tools/extract_ndxr_native_slices.py` extrait les records sélectionnés et
produit leurs contrats FNV/sha256 ;
`tools/make_mission01_native_manifest.py` construit un manifeste développeur
externe.

Le corpus local utilisé pour le smoke contient :

- F-16C LOD1 `root.1.m16.10` : 4 435 vertices, 6 468 indices, 12 polygones,
  stride 28 ;
- terrain `entry119/021_FHM/014_FHM/010_NDXR` : 1 300 vertices, 1 626 indices,
  21 polygones, stride 32.

Le manifeste passe `--validate-manifest`; `--present-manifest` réussit avec
une cible 1280×720 et exporte une capture native reproductible. La capture
montre bien une couverture géométrique native, mais reste clairsemée et non
texturée : elle ne constitue pas une comparaison retail.

## Validation

- test conditionnel terrain réel : succès ;
- test conditionnel F-16 LOD1 réel : succès ;
- manifeste externe réel : validation et présentation Vulkan réussies ;
- CTest Xvfb/dummy : 3/3 ;
- binaire : aucune chaîne Xbox/Xenia/RexGlue/PPC ;
- aucune archive n’est lue par le produit.

## Limites

Les attributs UV/normales, NTXR et permutations shader restent à raccorder.
La caméra/transformation de Mission 01 reste celle du runtime natif et non un
contrat oracle. La référence couleur/profondeur/replay de 1 800 ticks manque
toujours ; la gate de comparabilité retail demeure donc ouverte.
