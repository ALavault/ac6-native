# Cycle 943 — probe BC3 NTXR

Le probe Xenos tiled BC3 existant a décodé 11 des 13 NTXR de
`entry119/022_FHM` en PPM externes avec permutation `swap16`. Les images
256×256 produisent des pixels non nuls (maxima observés 117–255), ce qui
confirme que les payloads ne sont pas vides.

Deux textures avec dimensions/chaînes de mips différentes dépassent encore la
borne du probe (`IndexError`) et restent volontairement hors produit. Le
résultat est une preuve de données utiles, pas encore un contrat de sampling
retail : le tiling/mip layout complet doit être généralisé et validé avant de
brancher ces PPM aux bindings natifs.
