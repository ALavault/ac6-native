# Worker 1 — fermeture statique Mission 01

Racine qualifiée : `game-files/`. Extraction bornée des entrées DATA.TBL 9 et 119 exécutée vers `/tmp`; aucun payload retail n’est conservé dans le dépôt.

Identités : DATA.TBL `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`, DATA00.PAC `c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816`, DATA01.PAC `eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4`.

Résultat : entrées 9 et 119 décodées, racine FHM, respectivement 134 FHM et 978 feuilles pour l’entrée 9, puis 8 FHM et 495 feuilles pour l’entrée 119, `parse_failures=0`. Six slices NDXR de l’entrée 119 sont reprises avec SHA-256 exacts. Les associations transform/material/texture/caméra restent `open` faute d’identité exacte ; aucune note parser/FHM invalide n’est utilisée.

Validations : extraction `tools/extract_ac6_pac.py --indices 9 119 --decompress` réussie (`decoded=2`, `fhm=2`); inventaire slices `reports/ac6-mission01-retail-slices.tsv` vérifié ; `git status` contrôlé sans modification hors périmètre par cette tâche. Aucun commit.
