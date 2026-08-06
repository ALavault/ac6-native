# Cycle 940 — invariant de couverture du renderer

Le test produit vérifie désormais qu’un rendu géométrique accepté effectue au
moins un appel de géométrie, un triangle rasterisé et une écriture de pixel.
Cela complète le simple hash/readback et détecte une régression où la scène
serait acceptée mais entièrement silencieuse.

CTest normal : 3/3. Cette invariant ne transforme pas la couverture clairsemée
actuelle en preuve retail ; elle garantit seulement que le chemin natif soumis
reste actif.
