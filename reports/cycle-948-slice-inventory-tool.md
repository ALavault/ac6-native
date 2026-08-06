# Cycle 948 — vérificateur d’inventaire slices

Ajout de `verify_mission01_slice_inventory.py`. L’outil vérifie pour chaque
ligne le rôle/asset, l’existence du fichier, le SHA-256 et le contrat NDXR
(vertices, indices, primitives, stride), puis échoue au premier écart.

Run courant : `slice_inventory=pass rows=14`.
