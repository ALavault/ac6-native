# Cycle 893 — soumission géométrique complète native

Date : 2026-08-04

Le renderer natif ne limite plus les buffers qualifiés à quatre sommets et huit
indices. `NativeGeometryDatabase` décode désormais tous les éléments déclarés,
avec des plafonds explicites d’allocation (1 000 000 sommets, 4 000 000 indices),
et `NativeRenderTarget::draw_world_geometry` rasterise les triangles indexés
avec les règles de depth test/write et blend déjà qualifiées.

La voie de points diagnostique reste uniquement un support de géométrie
dégénérée/clippée ; elle ne remplace plus la soumission indexée.

Validation : build normal, CTest Xvfb/dummy 3/3 et ASan/UBSan 3/3. Les
fixtures synthétiques ont été ajustées pour vérifier les comptes complets.

Le loader reconnaît maintenant le format binaire big-endian observé : tables
d’objets/polygones, streams index/vertex, formats `0x06/0x07`, stride 32/44,
restart `0xffff`, floats et indices big-endian. Le buffer réel
`021_FHM/014_FHM/010_NDXR.ndxr` est accepté avec 1 300 vertices, 1 626 indices
bruts et 21 descripteurs via le test conditionnel `AC6_REAL_NDXR`.

Limites restantes : les layouts non `0x06/0x07`, les attributs UV/normales et les
textures NTXR ne sont pas encore décodés ; le shading reste donc contractuel,
pas pixel-identique retail.
