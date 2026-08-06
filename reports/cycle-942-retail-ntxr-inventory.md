# Cycle 942 — inventaire NTXR sky/cloud

L’inventaire local de `entry119/022_FHM` contient 13 NTXR retail. Leurs
identités et dimensions sont maintenant vérifiées par
`extract_ntxr_native_slices.py`; les payloads restent dans le répertoire de
preuves externe et ne sont pas copiés dans CPack.

Le format observé est Xenos tiled BC3 (format guest 20/0x020c selon la voie de
capture). Le runtime natif valide déjà les en-têtes, dimensions, format, taille
et hash ; le décodage texel tiled exact reste volontairement séparé du contrat
géométrie tant que le swizzle/endian n’est pas confirmé par une image positive.
