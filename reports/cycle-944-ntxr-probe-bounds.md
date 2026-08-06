# Cycle 944 — bornes du probe NTXR

Le probe BC3 tiled ne plante plus sur les chaînes de mips partielles : il
ignore les blocs dont les 16 octets ne sont pas présents et imprime
`decoded_blocks/skipped_blocks`. Les 13 NTXR sky/cloud produisent maintenant
une sortie bornée ; deux ressources signalent explicitement des blocs sautés,
au lieu d’être acceptées silencieusement.

Ce changement reste un outil d’inspection hors runtime. Les sorties partielles
ne sont pas promues en textures de parité sans contrat mip complet.
