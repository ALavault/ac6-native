# Cycle 935 — extensions de manifeste Mission 01

`make_mission01_native_manifest.py` accepte maintenant des entrées répétées
`--extra ASSET:SLICE:KIND:STABLE`. Elles alimentent automatiquement catalog,
launch, render, drawable, transform, material, texture, shader et buffers, ce
qui permet d’ajouter sky/cloud/map objects dès qu’un slice NDXR qualifié est
disponible.

Validation sur les slices binaires Mission 01 existants : manifeste généré avec
les assets 9, 119 et 165, puis `ac6-native --validate-manifest` retourne 0.
Aucun asset absent n’est fabriqué ni copié dans le produit.
