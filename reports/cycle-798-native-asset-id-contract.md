# Cycle 798 — contrat d’IDs d’assets natifs

`MissionAssetDatabase` accepte uniquement des IDs non nuls, chemins relatifs
de manifeste et SHA-256 non vides. Les doublons et entrées incomplètes sont
rejetés ; la résolution retourne un enregistrement stable ou `nullptr`.
Aucun chemin guest, pointeur PPC ou asset retail n’est embarqué dans le produit.

Validation : rebuild CMake et CTest `ac6-product-runtime-tests` (`1/1`).
