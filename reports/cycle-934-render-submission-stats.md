# Cycle 934 — statistiques de soumission natives

Le readback développeur expose désormais, sans modifier le produit rendu, les
compteurs `geometry_calls`, `raster_triangles` et `raster_writes`. Une capture
Mission 01 actuelle confirme `geometry=2`, `triangles=3190`, `writes=2156`,
`coverage=2004` : le noir est une couverture réellement clairsemée, pas un
échec silencieux du renderer.

Le mode est opt-in via `AC6_RENDER_STATS=1` et n'est pas requis par le paquet.
La scène externe ne contient encore que F-16/terrain ; sky/cloud et caméra
stock restent à qualifier avant toute comparaison oracle.
