# Cycle 799 — chargement de manifeste externe

Le produit natif charge désormais un manifeste TSV externe (`id`, chemin
relatif, SHA-256), avec rejet fail-closed des lignes mal formées ou
dupliquées. Le test crée un manifeste temporaire, le résout par l’ID 119 puis
le supprime ; aucune donnée retail n’est copiée dans le binaire.

Validation : CMake rebuild et CTest `1/1` réussi.
