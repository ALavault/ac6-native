# Cycle 949 — chemins texture relatifs

Les PPM fournis par `--texture` sont maintenant copiés dans le répertoire du
manifeste généré et référencés par basename relatif. Le hash FNV et la taille
restent ceux du payload source ; le loader valide ensuite le fichier relatif.

Validation : manifeste généré avec `sky000.ppm`, ligne `textures.tsv` relative,
et `ac6-native --validate-manifest` retourne 0. Le paquet produit ne contient
toujours aucun PPM retail.
